#!/bin/bash

set -e

script_dir=$(dirname "$0")
source "$script_dir/../ci_includes.generated.sh"

OSTYPE=$(uname)

if [ "${OSTYPE}" != "Darwin" ]; then
    echo "[Error] macOS package script can be run on Darwin-type OS only."
    exit 1
fi

echo "=> Preparing package build"
GIT_TAG=$(git describe --tags --long --always)
GIT_TAG_ONLY=$(git describe --tags --always)

echo "=> Modifying $PLUGIN_NAME.so"
mkdir -p lib

function copy_local_dylib
{
	local dylib
	otool -L $1 | awk '/^	\/usr\/local\/(opt|Cellar)\/.*\.dylib/{print $1}' |
	while read -r dylib; do
		echo "Changing dependency $1 -> $dylib"
		local b=$(basename $dylib)
		if test ! -e lib/$b; then
			cp $dylib lib/
			chmod +rwx lib/$b
			install_name_tool -id "@loader_path/$b" lib/$b
			copy_local_dylib lib/$b
		fi
		install_name_tool -change "$dylib" "@loader_path/../lib/$b" $1
	done
}

install_name_tool \
	-change /tmp/obsdeps/lib/QtWidgets.framework/Versions/5/QtWidgets \
		@executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets \
	-change /tmp/obsdeps/lib/QtGui.framework/Versions/5/QtGui \
		@executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui \
	-change /tmp/obsdeps/lib/QtCore.framework/Versions/5/QtCore \
		@executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore \
	./build/$PLUGIN_NAME.so

copy_local_dylib ./build/${PLUGIN_NAME}.so

# Check if replacement worked
for dylib in ./build/$PLUGIN_NAME.so lib/*.dylib ; do
	test -f "$dylib" || continue
	chmod +r $dylib
	echo "=> Dependencies for $(basename $dylib)"
	otool -L $dylib
	echo
	if [[ "$RELEASE_MODE" == "True" ]]; then
		echo "=> Signing plugin binary: $dylib"
		codesign --sign "$CODE_SIGNING_IDENTITY" $dylib
	else
		echo "=> Skipped plugin codesigning since RELEASE_MODE=$RELEASE_MODE"
	fi
done

echo "=> ZIP package build"
ziproot=package-zip/$PLUGIN_NAME
zipfile=${PLUGIN_NAME}-${GIT_TAG}-macos.zip
rm -rf ${ziproot:?}/
mkdir -p $ziproot/bin
cp ./build/$PLUGIN_NAME.so $ziproot/bin/
cp LICENSE data/LICENSE-$PLUGIN_NAME
cp dlib/LICENSE.txt data/LICENSE-dlib
cp -a data $ziproot/
mkdir -p ./release
rmdir lib || mv lib $ziproot/
(cd package-zip && zip -r ../release/$zipfile $PLUGIN_NAME)

echo "=> DMG package build"
if pip3 install dmgbuild || pip install dmgbuild; then
	sed \
		-e "s;%PLUGIN_NAME%;$PLUGIN_NAME;g" \
		-e "s;%VERSION%;${GIT_TAG_ONLY};g" \
		-e "s;%PLUGIN_ROOT%;$ziproot;g" \
		< ci/macos/package-dmg.json.template > package-dmg.json
	dmgbuild "$PLUGIN_NAME ${GIT_TAG}" "release/${PLUGIN_NAME}-${GIT_TAG}-macos.dmg" -s ./package-dmg.json
fi

# echo "=> Actual package build"
# packagesbuild ./installer/installer-macOS.generated.pkgproj

if [[ "$RELEASE_MODE" == "True" ]]; then
	:> requests

	for FILENAME in $zipfile ${PLUGIN_NAME}-${GIT_TAG}-macos.dmg; do
		echo "=> Submitting installer $FILENAME for notarization"
		UPLOAD_RESULT=$(xcrun altool \
			--notarize-app \
			--primary-bundle-id "$MACOS_BUNDLEID" \
			--username "$AC_USERNAME" \
			--password "$AC_PASSWORD" \
			--asc-provider "$AC_PROVIDER_SHORTNAME" \
			--file "./release/$FILENAME")

		REQUEST_UUID=$(echo $UPLOAD_RESULT | awk -F ' = ' '/RequestUUID/ {print $2}')
		echo "Request UUID: $REQUEST_UUID"
		echo "$REQUEST_UUID" >> requests
	done

	t=10
	for REQUEST_UUID in $(cat requests); do
		echo "=> Wait for notarization result of $REQUEST_UUID"
		# Pieces of code borrowed from rednoah/notarized-app
		while sleep $t && date; do
			CHECK_RESULT=$(xcrun altool \
				--notarization-info "$REQUEST_UUID" \
				--username "$AC_USERNAME" \
				--password "$AC_PASSWORD" \
				--asc-provider "$AC_PROVIDER_SHORTNAME")
			echo "$CHECK_RESULT"

			if ! grep -q "Status: in progress" <<< "$CHECK_RESULT"; then
				echo "$CHECK_RESULT" >> release/${PLUGIN_NAME}-${GIT_TAG}-macos-codesign.log
				if expr "$FILENAME" : '.*dmg$'; then
					echo "=> Staple ticket to installer: $FILENAME"
					xcrun stapler staple ./release/$FILENAME
				fi
				t=0
				break
			fi
			t=10
		done
	done
else
	echo "=> Skipped installer codesigning and notarization"
fi
