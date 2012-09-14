#!/bin/sh

set -v

aclocal
autoconf
autoheader
automake --add-missing --copy
automake
