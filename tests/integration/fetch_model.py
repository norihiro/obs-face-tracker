"""Fetches the face detector's model that the tests with a face need, into a
data directory laid out as the plugin's (dlib_hog_model/...), and prints the
directory. One that is there already is not fetched again.

Usage: python fetch_model.py [directory]
The directory is a folder of this user's cache by default.
"""

import bz2
import hashlib
import os
import platform
import sys
import urllib.request
from pathlib import Path

# As the CI workflow fetches it
HOG_URL = "https://github.com/norihiro/obs-face-tracker/releases/download/0.7.0-hogdata/frontal_face_detector.dat.bz2"
HOG_SHA256 = "f3f7aa833fb4a14a46fc48689f45b98d85ef72d6dfaf8bdc012b7d3c522a5426"


def default_directory():
    if platform.system() == "Darwin":
        cache = Path.home() / "Library" / "Caches"
    elif platform.system() == "Windows":
        cache = Path(os.environ.get("LOCALAPPDATA", Path.home()))
    else:
        cache = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return cache / "obs-face-tracker" / "models"


def fetch(directory: Path):
    model = directory / "dlib_hog_model" / "frontal_face_detector.dat"
    if model.is_file() and hashlib.sha256(model.read_bytes()).hexdigest() == HOG_SHA256:
        return
    with urllib.request.urlopen(HOG_URL) as response:
        data = bz2.decompress(response.read())
    if hashlib.sha256(data).hexdigest() != HOG_SHA256:
        raise RuntimeError(f"{HOG_URL} is not the model it was")
    model.parent.mkdir(parents=True, exist_ok=True)
    partial = model.with_suffix(".partial")
    partial.write_bytes(data)
    partial.replace(model)


if __name__ == "__main__":
    directory = Path(sys.argv[1]) if len(sys.argv) > 1 else default_directory()
    fetch(directory)
    print(directory)
