#! /bin/bash

set -ex

docker_image="$1"
rpmbuild="$2"

PLUGIN_NAME=$(awk '/^project\(/{print gensub(/project\(([^ ()]*).*/, "\\1", 1, $0)}' CMakeLists.txt)
PLUGIN_NAME_FEDORA="$(sed -e 's/^obs-/obs-studio-plugin-/' <<< "$PLUGIN_NAME")"
OBS_VERSION=$(docker run $docker_image bash -c 'rpm -q --qf "%{version}" obs-studio')
eval $(git describe --tag --always --long | awk '
BEGIN {
	VERSION="unknown";
	RELEASE=0;
}
{
	if (match($0, /^(.*)-([0-9]*)-g[0-9a-f]*$/, aa)) {
		VERSION = aa[1]
		RELEASE = aa[2]
	}
}
END {
	VERSION = gensub(/-(alpha|beta|rc)/, "~\\1", 1, VERSION);
	gsub(/["'\''-]/, ".", VERSION);
	printf("VERSION='\''%s'\'' RELEASE=%d\n", VERSION, RELEASE + 1);
}')

rm -rf $rpmbuild
mkdir -p $rpmbuild/{BUILD,BUILDROOT,SRPMS,SOURCES,SPECS,RPMS}
rpmbuild="$(cd $rpmbuild && pwd -P)"
chmod a+w $rpmbuild/{BUILD,BUILDROOT,SRPMS,RPMS}
test -x /usr/sbin/selinuxenabled && /usr/sbin/selinuxenabled && chcon -Rt container_file_t $rpmbuild

# Prepare files
sed \
	-e "s/@PLUGIN_NAME@/$PLUGIN_NAME/g" \
	-e "s/@PLUGIN_NAME_FEDORA@/$PLUGIN_NAME_FEDORA/g" \
	-e "s/@VERSION@/$VERSION/g" \
	-e "s/@RELEASE@/$RELEASE/g" \
	-e "s/@OBS_VERSION@/$OBS_VERSION/g" \
	< ci/plugin.spec \
	> $rpmbuild/SPECS/$PLUGIN_NAME_FEDORA.spec

git archive --format=tar --prefix=$PLUGIN_NAME_FEDORA-$VERSION/ HEAD | bzip2 > $rpmbuild/SOURCES/$PLUGIN_NAME_FEDORA-$VERSION.tar.bz2

docker run -v $rpmbuild:/home/rpm/rpmbuild $docker_image bash -c "
sudo dnf builddep -y ~/rpmbuild/SPECS/$PLUGIN_NAME_FEDORA.spec &&
sudo chown 0:0 ~/rpmbuild/SOURCES/* &&
sudo chown 0:0 ~/rpmbuild/SPECS/* &&
rpmbuild -ba ~/rpmbuild/SPECS/$PLUGIN_NAME_FEDORA.spec
"
