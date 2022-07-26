#! /bin/bash

set -e

url="$1"
sum="$2"
dir="$3"

b="$(basename "$url")"
t="/tmp/$b"

retry=5

for ((i=0; i<retry; i++)); do
        curl --location -o "$t" "$url"
        if sha256sum -c <<<"$sum $t"; then
                break;
        fi
        rm "$t"
        sleep $i
done

mkdir -p "$dir"
cd "$dir"
case "$b" in
        *.tar.xz)
                tar xJf "$t"
                ;;
        *.tar.gz)
                tar xzf "$t"
                ;;
        *)
                echo "Error: Unsupported format to extract $b" >&2
                exit 1
esac
xattr -r -d com.apple.quarantine ./

rm -f "$t"
