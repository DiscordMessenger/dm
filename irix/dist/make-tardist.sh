#!/bin/sh
# Make the Discord Messenger tardist (an inst(1M) distribution in a tar
# file) for IRIX 6.5.22 and later.  Runs on IRIX, with gendist(1M)
# (inst_dev, the Software Packager).
#
#   make-tardist.sh PROGRAM CACERT.PEM FONTS VERSION [OUTDIR]
#
# PROGRAM     the Motif client built for n32 MIPS III, everything but IRIX
#             linked in, DM_DATADIR=/usr/local/lib/discord-messenger
#             (Makefile.unix FRONTEND=motif STATIC_DEPS=1)
# CACERT.PEM  Mozilla's roots of trust (pkgsrc security/mozilla-rootcerts:
#             share/mozilla-rootcerts/cacert.pem)
# FONTS       a directory with DejaVuSans.ttf, -Bold, -Oblique,
#             -BoldOblique, DejaVuSansMono.ttf, -Bold and NotoColorEmoji.ttf
#             (pkgsrc's share/fonts/X11/TTF has them all)
# VERSION     the image version, a number that grows with each release
#             (for example 111000001: 1.11, build 1)
#
# Installs /usr/local/bin/discord-messenger and
# /usr/local/lib/discord-messenger (roots, fonts, licenses, README).
set -eu
# IRIX's /bin/sh is a Bourne shell: backquotes, not $(...).
here=`dirname "$0"`
here=`cd "$here" && pwd`
prog=$1 cacert=$2 fonts=$3 version=$4 out=${5:-`pwd`}
case $version in *[!0-9]*|"") echo "VERSION must be a number" >&2; exit 1 ;; esac

lib=usr/local/lib/discord-messenger
work=/usr/tmp/dmessenger-dist.$$
trap 'rm -rf "$work"' 0
mkdir -p "$work/src/usr/local/bin" "$work/src/$lib/licenses" "$work/src/$lib/fonts" "$work/dist"

cp "$prog" "$work/src/usr/local/bin/discord-messenger"
cp "$cacert" "$work/src/$lib/cacert.pem"
cp "$here/README" "$work/src/$lib/README"
cp "$here/licenses/"* "$work/src/$lib/licenses/"
for f in DejaVuSans DejaVuSans-Bold DejaVuSans-Oblique DejaVuSans-BoldOblique \
	DejaVuSansMono DejaVuSansMono-Bold NotoColorEmoji; do
	cp "$fonts/$f.ttf" "$work/src/$lib/fonts/"
done

# The idb: type mode owner group destination source tags, sorted on the
# destination and source (gendist(1M)).
(
	cd "$work/src"
	for d in $lib $lib/fonts $lib/licenses; do
		echo "d 0755 root sys $d $d DMESSENGER_BASE"
	done
	echo "f 0755 root sys usr/local/bin/discord-messenger usr/local/bin/discord-messenger DMESSENGER_BASE nostrip"
	find $lib -type f -print | while read f; do
		echo "f 0644 root sys $f $f DMESSENGER_BASE"
	done
) | sort +4u -6 > "$work/idb"

sed "s/VERSION/$version/" "$here/dmessenger.spec" > "$work/spec"

/usr/sbin/gendist -rbase / -sbase "$work/src" -idb "$work/idb" \
	-spec "$work/spec" -dist "$work/dist" -nostrip -all

name=dmessenger-$version.tardist
(cd "$work/dist" && tar cf - dmessenger*) > "$out/$name"
ls -l "$out/$name"
