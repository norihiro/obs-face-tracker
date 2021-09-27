# OBS Face Tracker Plugin

## Introduction

This plugin provide video filters for face detection and face tracking for mainly a speaking person.

This plugin employs [dlib](http://dlib.net/) on face detection and object tracking.
The frame of the source is periodically taken to face detection algorithm.
Once a face is found, the face is tracked.
Based on the location and the size of the face under tracking, the frame will be cropped.

## Usage
### Software Zoom Filter
The face tracker is implemented as an effect filter so that any video source can have the face tracker.
1. Open filters for a source on OBS Studio.
2. Click the add button on `Effect Filters`.
3. Add `Face Tracker`.

See [Properties](doc/properties.md) for the description of each property.

### PTZ Control
Experimental version of PTZ control is provided as an video filter.
1. Open filters for a source on OBS Studio,
2. Click the add button on `Audio/Video Filters`.
3. Add `Face Tracker PTZ`.

See [Properties](doc/properties-ptz.md) for the description of each property.

See [Limitations](https://github.com/norihiro/obs-face-tracker/wiki/PTZ-Limitation)
for current limitations of PTZ control feature.

## Wiki
- [Install procedure for macOS](https://github.com/norihiro/obs-face-tracker/wiki/Install-MacOS)
- [FAQ](https://github.com/norihiro/obs-face-tracker/wiki/FAQ)

## Building

This plugin requires [dlib](http://dlib.net/) to be built.
The `dlib` should be extracted under `obs-face-tracker` so that it will be linked statically.

For Linux and macOS,
expand `obs-face-tracker` outside `obs-studio` and expand `dlib` under `obs-face-tracker`.
Then, apply patch file to `dlib` so that dlib won't try to link `openblasp` but `openblas`.
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
cd dlib
patch -p1 < ../ci/common/dlib-cmake-no-openblasp.patch
cd ..
git clone https://github.com/norihiro/obs-ptz.git
cd obs-ptz
git checkout mod-for-ft
cd ..
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR=$d0/obs-studio/libobs -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
```

For Windows,
Build step would be as below, assuming you have OBS Studio successfully built at `%OBSPath%\build64`,
```
git clone https://github.com/norihiro/obs-face-tracker.git
cd obs-face-tracker
git clone https://github.com/davisking/dlib.git
mkdir build && cd build
cmake ^
-DLibObs_DIR="%OBSPath%\build64\libobs" ^
-DLIBOBS_INCLUDE_DIR="%OBSPath%\libobs" ^
-DLIBOBS_LIB="%OBSPath%\build64\libobs\%build_config%\obs.lib" ^
-DPTHREAD_LIBS="%OBSPath%\build64\deps\w32-pthreads\%build_config%\w32-pthreads.lib" ^
-DOBS_FRONTEND_LIB="%OBSPath%\build64\UI\obs-frontend-api\%build_config%\obs-frontend-api.lib" ..
make
```
For full build flow, see `azure-pipelines.yml`.

## Known issues
This plugin is heavily under development. So far these issues are under investigation.
- Memory usage is gradually increasing when continuously detecting faces.
- It consumes a lot of CPU resource.
- The frame sometimes vibrates because the face detection results vibrates.

## License
This plugin is licensed under GPLv2.

## Sponsor
- [Jimcom USA](https://www.jimcom.us/?ref=2) - a company of Live Streaming and Content Recording Professionals.
  Development of PTZ camera control is supported by Jimcom.
  Jimcom is now providing a 20% discount for their broadcast-quality network-connected PTZ cameras and free shipping in the USA.
  Visit [Jimcom USA](https://www.jimcom.us/?ref=2) and enter the coupon code **FACETRACK20** when you order.

## Acknowledgments
- [dlib](http://dlib.net/) - [git hub repository](https://github.com/davisking/dlib)
- [obz-ptz](https://github.com/glikely/obs-ptz) - PTZ camera interface is based on this code.
- [OBS Project](https://obsproject.com/)
