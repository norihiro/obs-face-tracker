#!/bin/bash
# Runs tests/integration against the plugins built in build/ with
# -DENABLE_TESTS=ON. See README.md. Arguments go to pytest.
#
# Env overrides: FACE_TRACKER_PLUGIN, FACE_TRACKER_TEST_SOURCES,
# FACE_TRACKER_OBS_BINARY, VENV

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
VENV="${VENV:-${XDG_CACHE_HOME:-$HOME/.cache}/obs-face-tracker-integration-venv}"

# The plugin called $1 under build/, a bundle or the plain library the build
# may make on macOS
find_plugin() {
	local found=
	[ "$(uname)" = Darwin ] && found="$(find "$ROOT/build" -maxdepth 4 -name "$1.plugin" 2>/dev/null | head -1)"
	[ -n "$found" ] || found="$(find "$ROOT/build" -maxdepth 4 -name "$1.so" 2>/dev/null | head -1)"
	if [ -z "$found" ]; then
		echo "error: no $1 under build/: build with -DENABLE_TESTS=ON, or set $2" >&2
		exit 1
	fi
	echo "$found"
}

FACE_TRACKER_PLUGIN="${FACE_TRACKER_PLUGIN:-$(find_plugin obs-face-tracker FACE_TRACKER_PLUGIN)}"
FACE_TRACKER_TEST_SOURCES="${FACE_TRACKER_TEST_SOURCES:-$(find_plugin face-tracker-test-sources FACE_TRACKER_TEST_SOURCES)}"
export FACE_TRACKER_PLUGIN FACE_TRACKER_TEST_SOURCES

if [ "$(uname)" = Darwin ] && pgrep -x OBS >/dev/null 2>&1; then
	echo "error: OBS is already running -- quit it first" >&2
	exit 1
fi

if ! "$VENV/bin/python" -c "import pytest, websockets" >/dev/null 2>&1; then
	python3 -m venv --clear "$VENV"
	"$VENV/bin/pip" install -q -r "$HERE/requirements.txt"
fi

cd "$HERE"
exec "$VENV/bin/pytest" "$@"
