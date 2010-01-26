#!/bin/sh

set -e || { echo "Couldn't set -e, exiting" >&2 ; exit 1 ; }
set -o nounset || { echo "Couldn't set -o nounset, exiting" >&2 ; exit 1 ; }

AUTOCONF=autoconf
AUTODIR=tools/autotools
CONFIGURE=`pwd`/configure

echo -n "Writing $CONFIGURE..."
"$AUTOCONF" "$AUTODIR/configure.ac" > "$CONFIGURE"
echo "ok"
