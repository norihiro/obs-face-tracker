# OBS integration tests

Boots a real OBS Studio with the plugin built from this checkout, and drives it
over obs-websocket, to test what can only be seen with the plugin running in OBS.
It is modelled on the suite of [obs-ptz](https://github.com/glikely/obs-ptz),
whose `tests/obs-integration` it borrows its way of starting OBS from.
`obsws.py`, the obs-websocket client, is a copy of obs-ptz's, which DistroAV's tests use as well.
Change it there first and copy it back, so that it stays the same in all of them.

## What it covers

`test_plugin.py` covers OBS loading the plugin, with the filters and sources it registers.

`test_faces.py` covers the face tracker finding a face, in what its filters show:
the Face Tracker filter framing a face that is off the middle,
and the Face Tracker PTZ filter drawing its markers on it.

The tests that need a face are skipped without `FACE_TRACKER_DATA`, the face detector's model.

## How it works

1. The build is made with `-DENABLE_TESTS=ON`, which adds a plugin of test sources, `face-tracker-test-sources` (`tests/test-sources`),
   which the tests load beside the face tracker, so that the face tracker is the same as the one released.
   `face_tracker_test_video_source` (`test-video-source.c`) is a source for the tracker to be a filter of:
   the tracker is an asynchronous video filter, which OBS puts only on an asynchronous source, and no source OBS has
   that is one can be made without a camera.
   It shows the raw BGRA frame in its `frame_file` setting, which the tests make from a photo
   of a face from dlib's examples (the `dlib` submodule), placed where a test wants it.
2. OBS is started with a throwaway profile, in a temporary home, so it does not read or change yours.
   On Windows it is the profile of a portable OBS kept for the tests, which each run replaces.
   obs-websocket is enabled in that profile.
   Without a display, on Linux, an `Xvfb` is started for it.
3. A test makes sources, and filters on them, through obs-websocket (`World` in `conftest.py`),
   and reads what they did from a screenshot of a source.
   The scenes and sources it made are removed after each test.

## Running

Build with the option, then run pytest with the plugin and OBS to use:

```bash
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build
```

```bash
tests/integration/run.sh
```

The tests need OBS 31 or later: the obs-websocket of an older one does not start, as it has its settings elsewhere.
They were run with:

- OBS 32.2.2 on macOS (Apple silicon);
- OBS 32.1.0, as Ubuntu 26.04 packages it, on Linux (arm64), with Xvfb;
- OBS 32.2.2 on Windows 11 (arm64), for arm64, and for x64 under emulation.

On macOS the build makes plain libraries, which the tests wrap in a `.plugin` bundle in the profile and sign ad hoc,
as OBS on Apple silicon loads bundles only.

`run.sh` makes a Python environment, finds the plugins in `build/`, and runs pytest, passing on its arguments.
These can be set to choose others:

| Variable | Meaning |
| --- | --- |
| `FACE_TRACKER_PLUGIN` | The plugin to test: the `.plugin` bundle on macOS, the `.so` on Linux, the `.dll` on Windows |
| `FACE_TRACKER_TEST_SOURCES` | The plugin of test sources, as `FACE_TRACKER_PLUGIN` |
| `FACE_TRACKER_OBS_BINARY` | The OBS to run; on Windows, which has to have it, the `obs64.exe` of a portable OBS for testing |
| `FACE_TRACKER_DATA` | A data directory with the detector's model, as `ci/download-dlib-models.sh` makes, for the tests that need a face |

OBS may already be running on macOS, but a second one asks whether to start anyway, which nothing answers,
so quit it first.
OBS's output is in the file the run prints the name of.

### Windows

`run.sh` is for macOS and Linux.
On Windows, set the variables above and run pytest in this directory, with a Python that has `requirements.txt`.

The OBS has to be one kept for the tests, as a copy of an OBS release's zip unpacked in a folder of its own:
the tests make it portable, replace its profile on each run, keeping the last as `config.prev`,
and copy both plugins into its `obs-plugins\64bit` and `data\obs-plugins`,
as OBS on Windows loads plugins from those only.
