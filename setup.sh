#!/bin/sh

############################################################################
# $Id$
# Quick script to set up a build environment from CVS
############################################################################
set -x
test ! -d aux && mkdir aux
aclocal -I aux && \
autoheader && \
autoconf && \
libtoolize --copy --automake --force && \
automake --add-missing --copy

