# Face Tracker Plugin for OBS Studio

## Introduction

This plugin provide a feature to track face of a person by detecting and tracking a face.

This plugin employs [dlib](http://dlib.net/) on face detection and object tracking.
The frame of the source is periodically taken to face detection algorithm.
Once a face is found, the face is tracked.
Based on the location and the size of the face under tracking, the frame will be cropped.

## Usage

For several use cases, total 3 methods are provided.

### Face Tracker Source
The face tracker is implemented as a source. You can easily have another source that tracks and zooms into a face.
1. Click the add button on the source list.
2. Add `Face Tracker`.
3. Scroll to the bottom and set `Source` property.

See [Properties](doc/properties.md) for the description of each property.

### Face Tracker Filter
The face tracker is implemented as an effect filter so that any video source can have the face tracker.
1. Open filters for a source on OBS Studio.
2. Click the add button on `Effect Filters`.
3. Add `Face Tracker`.

See [Properties](doc/properties.md) for the description of each property.

### Face Tracker PTZ
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
I modified `dlib` so that `openblasp` won't be linked but `openblas`.

For macOS,
install openblas and configure the path.
```
brew install openblas
export OPENBLAS_HOME=/usr/local/opt/openblas/
```

You will need CMake
install cmake by running the following:
```
brew install --cask cmake

```

For Linux and macOS,
expand `obs-face-tracker` outside `obs-studio` and build.
```
d0="$PWD"
git clone https://github.com/obsproject/obs-studio.git
mkdir obs-studio/build && cd obs-studio/build
cmake ..
make
cd "$d0"

git clone https://github.com/norihiro/obs-face-tracker.git
cd obs-face-tracker
git submodule update --init
mkdir build && cd build
cmake .. \
	-DLIBOBS_INCLUDE_DIR=$d0/obs-studio/libobs \
	-DLIBOBS_LIB=$d0/obs-studio/libobs \
	-DOBS_FRONTEND_LIB="$d0/obs-studio/build/UI/obs-frontend-api/libobs-frontend-api.dylib" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo
make
```

For Windows, see `.github/workflows/main.yml`.

## Preparing data file

You need to prepare a model file.

### HOG model file
Once you have built on Linux or macOS, you will find an executable file `face-detector-dlib-hog-datagen`.

Assuming your current directory is `obs-face-tracker`, run it like this.
```shell
mkdir data/dlib_hog_model/
./build/face-detector-dlib-hog-datagen > ./data/dlib_hog_model/frontal_face_detector.dat
```

### CNN model file
The CNN model file `mmod_human_face_detector.dat.bz2` can be downloaded from [dlib-models](https://github.com/davisking/dlib-models/).

Assuming your current directory is `obs-face-tracker`, run commands like below.
```shell
mkdir data/dlib_cnn_model/
git clone --depth 1 https://github.com/davisking/dlib-models
bunzip2 < dlib-models/mmod_human_face_detector.dat.bz2 > data/dlib_cnn_model/mmod_human_face_detector.dat
```

### 5-point face landmark model file
The 5-point face landmark model file `shape_predictor_5_face_landmarks.dat.bz2` can be downloaded from [dlib-models](https://github.com/davisking/dlib-models/).

Assuming your current directory is `obs-face-tracker`, run commands like below.
```shell
mkdir data/dlib_face_landmark_model/
git clone --depth 1 https://github.com/davisking/dlib-models
bunzip2 < dlib-models/shape_predictor_5_face_landmarks.dat.bz2 > data/dlib_face_landmark_model/shape_predictor_5_face_landmarks.dat
```

### 68-point face landmark model file
> [!NOTE]
> The 68-point face landmark model is a non-free license.
. Check [README](https://github.com/davisking/dlib-models/#shape_predictor_68_face_landmarksdatbz2) for the restriction.

If you want to use the 68-point face landmark model file `shape_predictor_68_face_landmarks.dat.bz2`, run commands like below.
```shell
mkdir data/dlib_face_landmark_model/
git clone --depth 1 https://github.com/davisking/dlib-models
bunzip2 < dlib-models/shape_predictor_68_face_landmarks.dat.bz2 > data/dlib_face_landmark_model/shape_predictor_68_face_landmarks.dat
```

### Installing the model files
Once you have prepared the model files under `data` directory,
run `cd build && make install` so that the data file will be installed.

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
- [obz-ptz](https://github.com/glikely/obs-ptz) - PTZ camera control goes through this plugin.
- [OBS Project](https://obsproject.com/)
