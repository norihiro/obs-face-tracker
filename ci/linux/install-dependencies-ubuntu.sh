#!/bin/sh
set -ex

sudo add-apt-repository -y ppa:obsproject/obs-studio
sudo apt-get -qq update

sudo apt-get install -y \
	libc-dev-bin \
	libc6-dev git \
	build-essential \
	checkinstall \
	cmake \
	obs-studio \
	qtbase5-dev \
	qtbase5-private-dev

# Dirty hack
obs_version="$(dpkg -s obs-studio | awk '$1=="Version:" {print gensub(/-.*/, "", 1, $2)}')"
echo obs-studio version is "$obs_version"
sudo wget -O /usr/include/obs/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/${obs_version}/UI/obs-frontend-api/obs-frontend-api.h

sudo ldconfig
