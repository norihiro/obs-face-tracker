#!/bin/bash
# Builds the plugin with -DENABLE_TESTS=ON and runs tests/integration against
# it, on macOS or Linux. See tests/integration/README.md. Arguments go to
# pytest.
#
# On macOS OBS's development files are those that an obs-ptz checkout beside
# this one has fetched into its .deps, as its own build does: libobs, the
# frontend API, and OBS's dependencies and Qt. On Linux they are the system's
# (libobs-dev and Qt 6 on Ubuntu).
#
# Env overrides:
#   BUILD_DIR          where to build (default: build/)
#   OBS_DEPS           macOS: the .deps folder to build against
#   FACE_TRACKER_DATA  the detector's model (default: fetched into a cache by
#                      tests/integration/fetch_model.py)
#   FACE_TRACKER_OBS_BINARY  the OBS to run (default on macOS: /Applications/OBS.app,
#                      else the newest "/Applications/OBS <version>.app")
#   VENV               as tests/integration/run.sh takes it

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"

die() {
	echo "error: $*" >&2
	exit 1
}

[ -f "$ROOT/dlib/CMakeLists.txt" ] || die "the submodules are not checked out: git submodule update --init --recursive"

cmake_args=(-G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TESTS=ON -DQT_VERSION=6)

if [ "$(uname)" = Darwin ]; then
	if [ -z "${OBS_DEPS:-}" ]; then
		# beside this checkout, or beside the main one if this is a worktree
		main="$(dirname "$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || echo "$ROOT/.git")")"
		for dir in "$ROOT/.." "$main/.."; do
			OBS_DEPS="$dir/obs-ptz/.deps"
			[ -d "$OBS_DEPS/Frameworks/libobs.framework" ] && break
		done
	fi
	[ -d "$OBS_DEPS/Frameworks/libobs.framework" ] ||
		die "no OBS development files in $OBS_DEPS: build obs-ptz beside this checkout, or set OBS_DEPS"
	deps="$(ls -d "$OBS_DEPS"/obs-deps-20*-universal 2>/dev/null | sort | tail -1)"
	qt="$(ls -d "$OBS_DEPS"/obs-deps-qt6-*-universal 2>/dev/null | sort | tail -1)"
	[ -n "$deps" ] && [ -n "$qt" ] || die "no obs-deps and Qt in $OBS_DEPS"
	if [ -z "${FACE_TRACKER_OBS_BINARY:-}" ] && [ ! -d /Applications/OBS.app ]; then
		app="$(ls -d /Applications/OBS\ [0-9]*.app 2>/dev/null | sort -V | tail -1)"
		[ -n "$app" ] || die "no OBS in /Applications: set FACE_TRACKER_OBS_BINARY"
		export FACE_TRACKER_OBS_BINARY="$app/Contents/MacOS/OBS"
	fi
	cmake_args+=(
		-DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
		-DQTDIR="$qt;$deps"
		-Dlibobs_DIR="$OBS_DEPS/Frameworks/libobs.framework/Resources/cmake"
		-Dobs-frontend-api_DIR="$OBS_DEPS/lib/cmake/obs-frontend-api"
		-DCMAKE_MODULE_PATH="$OBS_DEPS/cmake/finders"
	)
fi

cmake -S "$ROOT" -B "$BUILD_DIR" "${cmake_args[@]}"
cmake --build "$BUILD_DIR"

export FACE_TRACKER_PLUGIN="$BUILD_DIR/obs-face-tracker.so"
export FACE_TRACKER_TEST_SOURCES="$BUILD_DIR/tests/test-sources/face-tracker-test-sources.so"
if [ -z "${FACE_TRACKER_DATA:-}" ]; then
	FACE_TRACKER_DATA="$(python3 "$ROOT/tests/integration/fetch_model.py")"
	export FACE_TRACKER_DATA
fi

exec "$ROOT/tests/integration/run.sh" "$@"
