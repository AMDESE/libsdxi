#!/bin/bash
libtoolize
aclocal
automake --add-missing --force-missing
autoreconf --verbose --install --force
