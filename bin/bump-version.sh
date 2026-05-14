#!/bin/bash

# This script will bump the version in all the "authoritative" places.
# Test output should no longer care about this, but run `make test` afterward to make sure
# Usage: ./bump-version.sh 1a/01-dev 1.1.0

set -euo pipefail
cd "$(dirname "$0")/.."

COMPILER_VERSION="$1"
LANGUAGE_VERSION=$(echo "$COMPILER_VERSION" | cut -f1 -d/)
LIBRARY_VERSION="$2"

sed -Ei "s|DIALOG_VERSION=.+|DIALOG_VERSION=$COMPILER_VERSION|" src/Makefile
sed -Ei "s|\(library version\) Library version .+|(library version) Library version $LIBRARY_VERSION.|" stdlib.dg
sed -Ei "s|\(extension version\) Unit test runner .+|(extension version) Unit test runner $LIBRARY_VERSION by Susan Davis|" stdlib.dg

sed -Ei "s|    lang-ver: .+|    lang-ver: $LANGUAGE_VERSION|" manual/antora.yml
sed -Ei "s|    lib-ver: .+|    lib-ver: $LIBRARY_VERSION|" manual/antora.yml
