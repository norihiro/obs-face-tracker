#! /bin/bash

flg_nonfree=0

while (($# > 0)); do
	case "$1" in
		--nonfree)
			flg_nonfree=1
			shift ;;
		*)
			echo "Error: unknown option $1" >&2
			exit 1;;
	esac
done

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

if ((flg_nonfree)); then
	bunzip2 < dlib-models/shape_predictor_68_face_landmarks.dat.bz2 > ${DESTDIR}data/dlib_face_landmark_model/shape_predictor_68_face_landmarks.dat
	bunzip2 < dlib-models/shape_predictor_68_face_landmarks_GTX.dat.bz2 > ${DESTDIR}data/dlib_face_landmark_model/shape_predictor_68_face_landmarks_GTX.dat
	awk '/^##/{p=0} /^##.*shape_predictor_68/{p=1} p' dlib-models/README.md > ${DESTDIR}data/LICENSE-shape_predictor_68_face_landmarks
fi
