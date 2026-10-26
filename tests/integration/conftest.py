"""Fixtures that boot a real OBS Studio, with the plugin built from this
checkout, and drive it over obs-websocket for the whole test session.

The build has to be made with -DENABLE_TESTS=ON, which adds a plugin of test
sources (tests/test-sources), loaded beside the face tracker: the test video
source (tests/test-sources/test-video-source.c).
Tests show a face with it, put the face tracker's filters on it, and read what
they do from a screenshot of it.
OBS gets a throwaway profile, so a run does not read or change your own. On
Windows that is the profile of a portable OBS kept for testing, which each run
replaces.

Environment:
  FACE_TRACKER_PLUGIN  the plugin to test: the .plugin bundle on macOS, the .so
                       on Linux, the .dll on Windows. Without it OBS has to find
                       the plugin in its usual places.
  FACE_TRACKER_TEST_SOURCES  the plugin of test sources, as FACE_TRACKER_PLUGIN.
  FACE_TRACKER_OBS_BINARY  the OBS to run, else OBS.app or `obs`. On Windows,
                       which has to have it, the obs64.exe of an OBS that is
                       for testing.
  FACE_TRACKER_DATA    a data directory with the face detector's model, as
                       ci/download-dlib-models.sh makes (dlib_hog_model/...).
                       The tests that need a face are skipped without it.
"""

import json
import os
import platform
import select
import shutil
import signal
import socket
import subprocess
import tempfile
import time
from pathlib import Path

import pytest
from obsws import ObsWebSocketError, connect_when_ready

# obs-websocket RequestStatus::NotReady, which it answers with while OBS's
# frontend is still starting up, after the handshake has completed.
OBS_NOT_READY = 207

REPO_ROOT = Path(__file__).resolve().parents[2]
# A photo with one large face, from dlib's examples
FACE_IMAGE = REPO_ROOT / "dlib" / "examples" / "faces" / "Tom_Cruise_avp_2014_4.jpg"
FRAME_SIZE = (960, 540)

IS_WINDOWS = platform.system() == "Windows"

PLUGIN_NAME = "obs-face-tracker"
TEST_SOURCES_NAME = "face-tracker-test-sources"
TRACKER_KIND = "face_tracker_ptz"
VIDEO_SOURCE_KIND = "face_tracker_test_video_source"


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def obs_config_root(home: Path) -> Path:
    if IS_WINDOWS:
        # A portable OBS keeps its profile in the folder it is in, see
        # portable_obs_home()
        return home / "config" / "obs-studio"
    if platform.system() == "Darwin":
        return home / "Library" / "Application Support" / "obs-studio"
    return home / ".config" / "obs-studio"


def output_log(name):
    """Where a child process's output goes: a file, kept to look at after a
    failure. Not a pipe, as nothing would read it and the child would block
    once it filled up."""
    fd, path = tempfile.mkstemp(prefix=f"{name}-", suffix=".log")
    out = os.fdopen(fd, "w")
    out.log_path = path
    return out


def wait_for_port(host, port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=1):
                return
        except OSError:
            time.sleep(0.2)
    raise TimeoutError(f"nothing listening on {host}:{port} after {timeout}s")


def portable_obs_home(obs_binary):
    """Windows: where the OBS at obs_binary keeps its profile, for it to have a
    fresh one. OBS uses the user's own %APPDATA% however the environment says
    otherwise, unless portable_mode.txt is next to its bin folder, when it keeps
    everything in a config folder there. Only an install made for testing is
    one to put that in, so the folder the last run left is kept as config.prev
    for one run and the one before is gone."""
    home = Path(obs_binary).resolve().parents[2]
    (home / "portable_mode.txt").touch()
    config = home / "config"
    previous = home / "config.prev"
    if config.exists():
        if previous.exists():
            shutil.rmtree(previous)
        config.rename(previous)
    return home


def stop_process_tree(proc, timeout):
    """stop_process_group() where there are no process groups: asks proc's
    windows to close, as taskkill does without /F, and when it has not exited,
    ends it and what it started. Returns whether proc exited on its own."""
    deadline = time.time() + timeout
    exited = False
    while not exited and time.time() < deadline:
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T"], capture_output=True)
        try:
            proc.wait(timeout=1)
            exited = True
        except subprocess.TimeoutExpired:
            pass
    if not exited:
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], capture_output=True)
    proc.wait()
    return exited


def stop_process_group(proc, timeout, sig=signal.SIGTERM):
    """Sends sig to proc (started with start_new_session=True), waits for it
    to exit and ends what else is left in its process group. SIGKILLs it if it
    does not exit. Returns whether it exited on its own."""
    if IS_WINDOWS:
        return stop_process_tree(proc, timeout)
    proc.send_signal(sig)
    try:
        proc.wait(timeout=timeout)
        exited = True
    except subprocess.TimeoutExpired:
        exited = False
    for leftover in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(proc.pid, leftover)
        except (ProcessLookupError, PermissionError):
            # gone, or on macOS what is left of the group is not ours to signal
            break
        time.sleep(0.5)
    proc.wait()
    return exited


def start_xvfb(timeout=10):
    """Starts an Xvfb on a free display, and returns it and its DISPLAY.
    -displayfd has Xvfb pick the display and say which when it is ready."""
    read_fd, write_fd = os.pipe()
    try:
        with output_log("xvfb") as out:
            proc = subprocess.Popen(
                ["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "1920x1080x24", "-nolisten", "tcp"],
                stdout=out, stderr=subprocess.STDOUT, pass_fds=(write_fd,), start_new_session=True)
        os.close(write_fd)
        write_fd = None
        number = b""
        deadline = time.time() + timeout
        while not number.endswith(b"\n"):
            ready, _, _ = select.select([read_fd], [], [], max(0, deadline - time.time()))
            chunk = os.read(read_fd, 16) if ready else b""
            if not chunk:
                stop_process_group(proc, timeout=5)
                raise RuntimeError("Xvfb never said which display it is on")
            number += chunk
        return proc, f":{number.decode().strip()}"
    finally:
        os.close(read_fd)
        if write_fd is not None:
            os.close(write_fd)


def install_data(dest: Path):
    data = os.environ.get("FACE_TRACKER_DATA")
    if data:
        shutil.copytree(data, dest, dirs_exist_ok=True)


def have_detector_model():
    data = os.environ.get("FACE_TRACKER_DATA")
    return bool(data) and (Path(data) / "dlib_hog_model" / "frontal_face_detector.dat").is_file()


def write_face_frame(path, center, face_height=0.55):
    """Writes, as raw BGRA, a frame with the face of FACE_IMAGE on black with
    its center at center, as fractions of the frame's width and height from its
    top left. Returns the size."""
    from PIL import Image  # only these tests need it

    width, height = FRAME_SIZE
    face = Image.open(FACE_IMAGE).convert("RGB")
    scale = face_height * height / face.height
    face = face.resize((round(face.width * scale), round(face.height * scale)))
    frame = Image.new("RGB", FRAME_SIZE)
    frame.paste(face, (round(center[0] * width - face.width / 2), round(center[1] * height - face.height / 2)))
    r, g, b = frame.split()
    path.write_bytes(Image.merge("RGBA", (b, g, r, Image.new("L", FRAME_SIZE, 255))).tobytes())
    return FRAME_SIZE


def install_module(plugins: Path, path: Path, name, data=None):
    """Puts the plugin at path in plugins, the folder OBS looks for one in, as
    name, with the data directory data if it has one. On Windows plugins is the
    folder of the OBS itself, which loads plugins from its own folders only."""
    if IS_WINDOWS:
        # Replaces what the last run put there, as it is an OBS for testing
        shutil.copy(path, plugins / "obs-plugins" / "64bit" / f"{name}.dll")
        datadir = plugins / "data" / "obs-plugins" / name
        if datadir.exists():
            shutil.rmtree(datadir)
        datadir.mkdir(parents=True)
        if data:
            data(datadir)
    elif platform.system() == "Darwin" and path.is_dir():
        plugins.mkdir(parents=True, exist_ok=True)
        (plugins / path.name).symlink_to(path)
    elif platform.system() == "Darwin":
        # A plain library, as the build makes it, in a bundle: OBS on Apple
        # silicon only looks for bundles
        bundle = plugins / f"{name}.plugin"
        macos = bundle / "Contents" / "MacOS"
        macos.mkdir(parents=True, exist_ok=True)
        (bundle / "Contents" / "Resources").mkdir()
        shutil.copy(path, macos / name)
        (bundle / "Contents" / "Info.plist").write_text(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<plist version="1.0"><dict>'
            f'<key>CFBundleExecutable</key><string>{name}</string>'
            f'<key>CFBundleIdentifier</key><string>test.{name}</string>'
            '<key>CFBundlePackageType</key><string>BNDL</string>'
            '</dict></plist>\n')
        if data:
            data(bundle / "Contents" / "Resources")
        subprocess.run(["codesign", "--force", "--sign", "-", str(bundle)], check=True, capture_output=True)
    else:
        # OBS skips a symlink here, so copy it
        bindir = plugins / name / "bin" / "64bit"
        bindir.mkdir(parents=True, exist_ok=True)
        shutil.copy(path, bindir / f"{name}.so")
        (bindir / f"{name}.so").chmod(0o755)
        if data:
            data(plugins / name / "data")


def install_plugin(home: Path, env):
    """Puts the plugin under test, and the plugin of test sources, where OBS
    looks for one."""
    plugins = home if IS_WINDOWS else obs_config_root(home) / "plugins"
    for var, name, data in (("FACE_TRACKER_PLUGIN", PLUGIN_NAME, install_data),
                            ("FACE_TRACKER_TEST_SOURCES", TEST_SOURCES_NAME, None)):
        path = os.environ.get(var)
        if not path:
            continue
        path = Path(path).resolve()
        if not path.exists():
            raise RuntimeError(f"no plugin at {path}: set {var}")
        install_module(plugins, path, name, data)


def isolate_macos_obs(home: Path, env):
    """Point OBS on macOS at home instead of the real user's.

    OBS asks the system for the account's home directory, which ignores $HOME.
    Core Foundation honors CFFIXED_USER_HOME, which gives OBS an empty profile.
    OBS then opens a dialog asking for permissions it will never be given,
    modal, unless it has been told it has shown it already.
    """
    env["CFFIXED_USER_HOME"] = str(home)
    with open(obs_config_root(home) / "global.ini", "a") as f:
        f.write("\n[General]\nMacOSPermissionsDialogLastShown=1000\n")


def write_websocket_config(home: Path, port, password):
    """Enable obs-websocket in the profile"""
    root = obs_config_root(home)
    root.mkdir(parents=True, exist_ok=True)
    config = root / "plugin_config" / "obs-websocket"
    config.mkdir(parents=True, exist_ok=True)
    (config / "config.json").write_text(json.dumps({
        "alerts_enabled": False,
        "auth_required": False,
        "first_load": False,
        "server_enabled": True,
        "server_password": password,
        "server_port": port,
    }))


def bounding_box(image, keep, thickness=1):
    """The box of the pixels of an RGB image that keep() says yes to, as
    (left, top, right, bottom), or None. Lines thinner than thickness are
    not counted, such as the cross that marks where a face is to be."""
    from PIL import Image, ImageFilter

    pixels = image.load()
    mask = Image.new("L", image.size, 0)
    out = mask.load()
    for y in range(image.height):
        for x in range(image.width):
            if keep(*pixels[x, y][:3]):
                out[x, y] = 255
    if thickness > 1:
        mask = mask.filter(ImageFilter.MinFilter(thickness))
    box = mask.getbbox()
    return box and (box[0], box[1], box[2] - 1, box[3] - 1)


def is_marker(r, g, b):
    """A pixel of the green box that marks a face the tracker follows"""
    return g > 180 and r < 90 and b < 90


def is_photo(r, g, b):
    """A pixel of the photo of the face, which is on black"""
    return max(r, g, b) > 30 and not is_marker(r, g, b)


def wait_for(predicate, timeout=5, interval=0.1):
    """Polls until predicate is true. One that raises, such as for a file that
    is not there yet, is not true yet, and the last exception is raised if time
    runs out, so a bug in it is not hidden as a timeout."""
    deadline = time.time() + timeout
    last_exc = None
    while time.time() < deadline:
        try:
            if predicate():
                return
        except Exception as e:
            last_exc = e
        time.sleep(interval)
    if last_exc is not None:
        raise last_exc
    raise AssertionError("condition never became true")


class Tracker:
    """A face_tracker_ptz filter on a source."""

    def __init__(self, world, source, name):
        self.world = world
        self.source = source
        self.name = name

    def settings(self):
        return self.world.ws.call("GetSourceFilter", {"sourceName": self.source, "filterName": self.name})[
            "filterSettings"]

    def configure(self, **settings):
        self.world.ws.call("SetSourceFilterSettings", {
            "sourceName": self.source, "filterName": self.name, "filterSettings": settings, "overlay": True})

    def remove(self):
        self.world.ws.call("RemoveSourceFilter", {"sourceName": self.source, "filterName": self.name})


class World:
    wait_for = staticmethod(wait_for)

    def __init__(self, ws, tmp):
        self.ws = ws
        self.tmp = tmp
        self.counter = 0
        self.scenes = []

    def screenshot(self, source, size=FRAME_SIZE):
        """What a source shows, with its filters, as an RGB image"""
        import base64
        import io

        from PIL import Image

        r = self.ws.call("GetSourceScreenshot", {"sourceName": source, "imageFormat": "png", "imageWidth": size[0],
                                                 "imageHeight": size[1]})
        return Image.open(io.BytesIO(base64.b64decode(r["imageData"].split(",", 1)[1]))).convert("RGB")

    def unique(self, prefix):
        self.counter += 1
        return f"{prefix}-{self.counter}"

    def create_scene(self):
        name = self.unique("ft-test-scene")
        self.ws.call("CreateScene", {"sceneName": name})
        self.scenes.append(name)
        return name

    def create_input(self, kind, name, settings=None):
        """Creates an input in a new scene and returns its UUID"""
        res = self.ws.call("CreateInput", {
            "sceneName": self.create_scene(),
            "inputName": name,
            "inputKind": kind,
            "inputSettings": settings or {},
            "sceneItemEnabled": True,
        })
        return res["inputUuid"]

    def make_face_source(self, center=(0.5, 0.5), face_height=0.55):
        """Makes a video source showing a face, in a scene that is made the
        program, so that it is rendered, and returns its name."""
        name = self.unique("face-source")
        frame = self.tmp / f"{name}.bgra"
        width, height = write_face_frame(frame, center, face_height)
        scene = self.create_scene()
        self.ws.call("CreateInput", {
            "sceneName": scene, "inputName": name, "inputKind": VIDEO_SOURCE_KIND,
            "inputSettings": {"frame_file": str(frame), "width": width, "height": height},
            "sceneItemEnabled": True,
        })
        self.ws.call("SetCurrentProgramScene", {"sceneName": scene})
        return name

    def make_tracker(self, source=None, **settings):
        """Adds a face_tracker_ptz filter to a source, a new one if none is
        given. It has to be an asynchronous video source, as the filter is."""
        if source is None:
            source = self.unique("tracker-source")
            self.create_input(VIDEO_SOURCE_KIND, source)
        name = self.unique("tracker")
        self.ws.call("CreateSourceFilter", {
            "sourceName": source, "filterName": name, "filterKind": TRACKER_KIND, "filterSettings": settings})
        return Tracker(self, source, name)

    def cleanup(self):
        for scene in self.scenes:
            try:
                # removes the sources the test made in it as well
                for item in self.ws.call("GetSceneItemList", {"sceneName": scene})["sceneItems"]:
                    self.ws.call("RemoveInput", {"inputName": item["sourceName"]})
                self.ws.call("RemoveScene", {"sceneName": scene})
            except ObsWebSocketError:
                pass
        self.scenes = []


@pytest.fixture(scope="session")
def obs_world(tmp_path_factory):
    obs_binary = os.environ.get("FACE_TRACKER_OBS_BINARY")
    if not obs_binary:
        if IS_WINDOWS:
            raise RuntimeError("set FACE_TRACKER_OBS_BINARY to the obs64.exe of an OBS for testing, "
                               "whose profile the tests replace")
        obs_binary = "/Applications/OBS.app/Contents/MacOS/OBS" if platform.system() == "Darwin" else "obs"

    home = portable_obs_home(obs_binary) if IS_WINDOWS else tmp_path_factory.mktemp("obs-home")
    ws_port = free_port()
    ws_password = "face-tracker-test-password"
    write_websocket_config(home, ws_port, ws_password)
    # OBS that has not run before opens its Auto-Configuration Wizard, a
    # modal dialog over the main window, which is closed on Windows to quit OBS
    (obs_config_root(home) / "user.ini").write_text("[General]\nFirstRun=true\n")

    env = dict(os.environ)
    env["HOME"] = str(home)
    env["OBS_WEBSOCKET_SERVER_ENABLE"] = "true"
    env["OBS_WEBSOCKET_SERVER_PORT"] = str(ws_port)
    env["OBS_WEBSOCKET_SERVER_PASSWORD"] = ws_password
    if platform.system() == "Darwin":
        isolate_macos_obs(home, env)
    install_plugin(home, env)

    # Started here and not through xvfb-run, whose SIGTERM leaves OBS running
    xvfb = None
    if platform.system() == "Linux" and not env.get("DISPLAY") and shutil.which("Xvfb"):
        xvfb, env["DISPLAY"] = start_xvfb()

    # Its own process group, so that what OBS starts goes when it does. On
    # Windows OBS looks for what it needs from the folder it is started in,
    # and logs to its profile, not its output
    with output_log("obs") as out:
        print(f"OBS log: {obs_config_root(home) / 'logs' if IS_WINDOWS else out.log_path}")
        proc = subprocess.Popen([obs_binary, "--disable-updater"], cwd=Path(obs_binary).parent if IS_WINDOWS else home,
                                env=env, stdout=out, stderr=subprocess.STDOUT,
                                **({} if IS_WINDOWS else {"start_new_session": True}))

    ws = None
    try:
        try:
            wait_for_port("127.0.0.1", ws_port, timeout=90)
        except TimeoutError as e:
            # The obs-websocket of OBS before 31 has its settings elsewhere
            raise RuntimeError(f"obs-websocket did not start: {e}. Is the OBS 31 or later, as the tests need?") from e
        ws = connect_when_ready(f"ws://127.0.0.1:{ws_port}", password=ws_password, timeout=30,
                                alive=lambda: proc.poll() is None)

        # The server answers before OBS's frontend has loaded, and rejects
        # requests until it has, or does not answer for a while if OBS is busy
        # starting up, which a first start in a new profile can be
        deadline = time.time() + 90
        while True:
            try:
                ws.call("GetSceneList", timeout=5)
                break
            except TimeoutError:
                if time.time() > deadline:
                    raise
            except ObsWebSocketError as e:
                if e.status.get("code") != OBS_NOT_READY or time.time() > deadline:
                    raise
                time.sleep(0.2)

        # Still slow to answer for a while, on a software renderer
        kinds = ws.call("GetInputKindList", timeout=30)["inputKinds"]
        if VIDEO_SOURCE_KIND not in kinds:
            raise RuntimeError(f"OBS has no {VIDEO_SOURCE_KIND}: is the build made with -DENABLE_TESTS=ON, "
                               "did OBS load its plugin of test sources (see FACE_TRACKER_TEST_SOURCES), "
                               "and is the OBS no older than the one the build is for?")

        yield World(ws, tmp_path_factory.mktemp("logs"))
    finally:
        if ws is not None:
            ws.close()
        # SIGINT closes the main window, a normal quit, which SIGTERM does
        # only since OBS 32.1
        stop_process_group(proc, timeout=10, sig=signal.SIGINT)
        if xvfb is not None:
            stop_process_group(xvfb, timeout=5)


@pytest.fixture
def world(obs_world):
    yield obs_world
    obs_world.cleanup()
