#! /bin/bash

mkdir -p ${DESTDIR}data

mkdir ${DESTDIR}data/dlib_hog_model
curl -LO https://github.com/norihiro/obs-face-tracker/releases/download/0.7.0-hogdata/frontal_face_detector.dat.bz2
bunzip2 < frontal_face_detector.dat.bz2 > ${DESTDIR}data/dlib_hog_model/frontal_face_detector.dat
git clone --depth 1 https://github.com/davisking/dlib-models
mkdir ${DESTDIR}data/{dlib_cnn_model,dlib_face_landmark_model}
bunzip2 < dlib-models/mmod_human_face_detector.dat.bz2 > ${DESTDIR}data/dlib_cnn_model/mmod_human_face_detector.dat
bunzip2 < dlib-models/shape_predictor_5_face_landmarks.dat.bz2 > ${DESTDIR}data/dlib_face_landmark_model/shape_predictor_5_face_landmarks.dat
cp dlib/LICENSE.txt ${DESTDIR}data/LICENSE-dlib
cp dlib-models/LICENSE ${DESTDIR}data/LICENSE-dlib-models
