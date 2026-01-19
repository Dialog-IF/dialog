#!/bin/bash

# This script will ump the version in all the "authoritative" places.
# You may also need to regenerate test output that embeds the version.

set -euo pipefail
cd "$(dirname "$0")/.."

COMPILER_VERSION="$1"
LANGUAGE_VERSION=$(echo "$COMPILER_VERSION" | cut -f1 -d/)
LIBRARY_VERSION="$2"

sed -Ei "s|DIALOG_VERSION=.+|DIALOG_VERSION=$COMPILER_VERSION|" src/Makefile
sed -Ei "s|\(library version\) Library version .+|(library version) Library version $LIBRARY_VERSION.|" stdlib.dg

sed -Ei "s|    lang-ver: .+|    lang-ver: $LANGUAGE_VERSION|" manual/antora.yml
sed -Ei "s|    lib-ver: .+|    lib-ver: $LIBRARY_VERSION|" manual/antora.yml
