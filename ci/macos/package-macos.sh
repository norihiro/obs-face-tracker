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
GIT_HASH=$(git rev-parse --short HEAD)
GIT_BRANCH_OR_TAG=$(git name-rev --name-only HEAD | awk -F/ '{print $NF}')

PKG_VERSION="$GIT_HASH-$GIT_BRANCH_OR_TAG"

FILENAME_UNSIGNED="$PLUGIN_NAME-$PKG_VERSION-Unsigned.pkg"
FILENAME="$PLUGIN_NAME-$PKG_VERSION.pkg"

echo "=> Modifying $PLUGIN_NAME.so"
mkdir lib

function copy_local_dylib
{
	local dylib
	otool -L $1 | awk '/^	\/usr\/local\/(opt|Cellar)\/.*\.dylib/{print $1}' |
	while read dylib; do
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
	echo "=> Dependencies for $(basename $dylib)"
	otool -L $dylib
	echo "=> Search paths written in $(basename $dylib)"
	otool -l $dylib
	echo
done

if [[ "$RELEASE_MODE" == "True" ]]; then
	echo "=> Signing plugin binary: $PLUGIN_NAME.so"
	codesign --sign "$CODE_SIGNING_IDENTITY" ./build/$PLUGIN_NAME.so
else
	echo "=> Skipped plugin codesigning"
fi

echo "=> ZIP package build"
ziproot=package-zip/$PLUGIN_NAME
zipfile=${PLUGIN_NAME}-${GIT_HASH}-macos.zip
mkdir -p $ziproot/bin
cp ./build/$PLUGIN_NAME.so $ziproot/bin/
cp -a data $ziproot/
mkdir -p ./release
chmod +x lib/*.dylib
mv lib $ziproot/
(cd package-zip && zip -r ../release/$zipfile $PLUGIN_NAME)

# echo "=> Actual package build"
# packagesbuild ./installer/installer-macOS.generated.pkgproj

# echo "=> Renaming $PLUGIN_NAME.pkg to $FILENAME"
# mv ./release/$PLUGIN_NAME.pkg ./release/$FILENAME_UNSIGNED

if [[ "$RELEASE_MODE" == "True" ]]; then
	echo "=> Signing installer: $FILENAME"
	productsign \
		--sign "$INSTALLER_SIGNING_IDENTITY" \
		./release/$FILENAME_UNSIGNED \
		./release/$FILENAME
	rm ./release/$FILENAME_UNSIGNED

	echo "=> Submitting installer $FILENAME for notarization"
	zip -r ./release/$FILENAME.zip ./release/$FILENAME
	UPLOAD_RESULT=$(xcrun altool \
		--notarize-app \
		--primary-bundle-id "$MACOS_BUNDLEID" \
		--username "$AC_USERNAME" \
		--password "$AC_PASSWORD" \
		--asc-provider "$AC_PROVIDER_SHORTNAME" \
		--file "./release/$FILENAME.zip")
	rm ./release/$FILENAME.zip

	REQUEST_UUID=$(echo $UPLOAD_RESULT | awk -F ' = ' '/RequestUUID/ {print $2}')
	echo "Request UUID: $REQUEST_UUID"

	echo "=> Wait for notarization result"
	# Pieces of code borrowed from rednoah/notarized-app
	while sleep 30 && date; do
		CHECK_RESULT=$(xcrun altool \
			--notarization-info "$REQUEST_UUID" \
			--username "$AC_USERNAME" \
			--password "$AC_PASSWORD" \
			--asc-provider "$AC_PROVIDER_SHORTNAME")
		echo $CHECK_RESULT

		if ! grep -q "Status: in progress" <<< "$CHECK_RESULT"; then
			echo "=> Staple ticket to installer: $FILENAME"
			xcrun stapler staple ./release/$FILENAME
			break
		fi
	done
else
	echo "=> Skipped installer codesigning and notarization"
fi
