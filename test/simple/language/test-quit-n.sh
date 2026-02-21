#!/bin/sh

../../../src/dgdebug -q --no-warn-not-topic test-quit-0.dg || exit 1
../../../src/dgdebug -q --no-warn-not-topic test-quit-1.dg && exit 1
exit 0
