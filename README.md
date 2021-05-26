# OBS Face Tracker Plugin

## Introduction

This plugin provide a filter for face detection and face tracking for mainly a speaking person.

This plugin employs [dlib](http://dlib.net/) on face detection and object tracking.
The frame of the source is periodically taken to face detection algorithm.
Once a face is found, the face is tracked.
Based on the location and the size of the face under tracking, the frame will be cropped.

## Building

This plugin requires [dlib](http://dlib.net/) to be built.
The `dlib` should be extracted under `obs-face-tracker` so that it will be linked statically.

```
d0="$PWD"
git clone https://github.com/obsproject/obs-studio.git
mkdir obs-studio/build && cd obs-studio/build
cmake ..
make
cd "$d0"

git clone https://github.com/norihiro/obs-face-tracker.git
cd obs-face-tracker
git clone https://github.com/davisking/dlib.git
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR=$d0/obs-studio/libobs -DCMAKE_BUILD_TYPE=RelWithDebInfo  ..
make
```

## License
This plugin is licensed under GPLv2.

## Acknowledgments
- [dlib](http://dlib.net/) - [git hub repository](https://github.com/davisking/dlib)
- [OBS Project](https://obsproject.com/)
