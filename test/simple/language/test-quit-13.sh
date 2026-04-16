#!/bin/sh

if ../../../src/dgdebug -q --no-warn-not-topic test-quit-13.dg; then
    exit 1;
elif [ $? -eq 13 ]; then
    exit 0;
else
    exit 1;
fi
