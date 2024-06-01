Name: @PLUGIN_NAME_FEDORA@
Version: @VERSION@
Release: @RELEASE@%{?dist}
Summary: OBS Studio plugin as video filters to track face for mainly a speaking person
License: GPLv3+

Source0: %{name}-%{version}.tar.bz2
Requires: obs-studio >= @OBS_VERSION@
BuildRequires: cmake, gcc, gcc-c++
BuildRequires: obs-studio-devel
BuildRequires: qt6-qtbase-devel qt6-qtbase-private-devel
BuildRequires: dlib-devel

%description
This plugin tracks face of a person by detecting and tracking a face.

This plugin employs dlib on face detection and object tracking. The frame of
the source is periodically taken to face detection algorithm. Once a face is
found, the face is tracked. Based on the location and the size of the face
under tracking, the frame will be cropped.

%prep
%autosetup -p1

%build
%{cmake} -DLINUX_PORTABLE=OFF -DLINUX_RPATH=OFF -DQT_VERSION=6 -DWITH_DLIB_SUBMODULE=OFF
%{cmake_build}

%install
%{cmake_install}

%files
%{_libdir}/obs-plugins/@PLUGIN_NAME@.so
%{_datadir}/obs/obs-plugins/@PLUGIN_NAME@/
