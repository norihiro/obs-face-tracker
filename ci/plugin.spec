Name: @PLUGIN_NAME_FEDORA@
Version: @VERSION@
Release: @RELEASE@%{?dist}
Summary: OBS Studio plugin as video filters to track face for mainly a speaking person
License: GPLv3+

Source0: %{name}-%{version}.tar.bz2
Source1: %{name}-%{version}-libvisca.tar.bz2
Source2: %{name}-%{version}-dlib-models.tar.bz2
Requires: obs-studio >= @OBS_VERSION@
BuildRequires: cmake, gcc, gcc-c++
BuildRequires: obs-studio-devel
BuildRequires: qt6-qtbase-devel qt6-qtbase-private-devel
BuildRequires: dlib-devel ffmpeg-free-devel sqlite-devel blas-devel lapack-devel
BuildRequires: flexiblas-devel
BuildRequires: libcurl-devel
# dlib-devel requires /usr/include/ffmpeg so that install ffmpeg-free-devel

%package data
Summary: Model file for %{name}
BuildArch: noarch
License: CC0-1.0

%package data-nonfree
Summary: Non-free model file for %{name}
BuildArch: noarch
License: Nonfree

%description
This plugin tracks face of a person by detecting and tracking a face.

This plugin employs dlib on face detection and object tracking. The frame of
the source is periodically taken to face detection algorithm. Once a face is
found, the face is tracked. Based on the location and the size of the face
under tracking, the frame will be cropped.

%description data
Model files for @PLUGIN_NAME_FEDORA@.
The model files came from https://github.com/davisking/dlib-models/.

%description data-nonfree
Non-free model files for @PLUGIN_NAME_FEDORA@.
The model file came from https://github.com/davisking/dlib-models/.

%prep
%autosetup -p1
%setup -T -D -a 1
%setup -T -D -a 2

%build
%{cmake} \
 -DLINUX_PORTABLE=OFF -DLINUX_RPATH=OFF \
 -DQT_VERSION=6 \
 -DWITH_DLIB_SUBMODULE=OFF \
 -DBUILD_SHARED_LIBS:BOOL=OFF
%{cmake_build}

%install
%{cmake_install}

mkdir -p %{buildroot}/%{_datadir}/licenses/%{name}/
mkdir -p %{buildroot}/%{_datadir}/licenses/%{name}-data/
mkdir -p %{buildroot}/%{_datadir}/licenses/%{name}-data-nonfree/
cp LICENSE %{buildroot}/%{_datadir}/licenses/%{name}/
mv %{buildroot}/%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/LICENSE-dlib %{buildroot}/%{_datadir}/licenses/%{name}/
mv %{buildroot}/%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/LICENSE-dlib-models %{buildroot}/%{_datadir}/licenses/%{name}-data/
mv %{buildroot}/%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/LICENSE-shape_predictor_68_face_landmarks %{buildroot}/%{_datadir}/licenses/%{name}-data-nonfree/

%files
%{_libdir}/obs-plugins/@PLUGIN_NAME@.so
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/locale/
%{_datadir}/licenses/%{name}/*

%files data
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/dlib_cnn_model
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/dlib_face_landmark_model/shape_predictor_5_face_landmarks.dat
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/dlib_hog_model
%{_datadir}/licenses/%{name}-data/*

%files data-nonfree
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/dlib_face_landmark_model/shape_predictor_68_face_landmarks.dat
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/dlib_face_landmark_model/shape_predictor_68_face_landmarks_GTX.dat
%{_datadir}/licenses/%{name}-data-nonfree/*
