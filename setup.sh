#!/bin/sh

############################################################################
# $Id$
# Quick script to set up a build environment from CVS
############################################################################
echo aclocal && aclocal && \
echo autoheader && autoheader && \
echo autoconf && autoconf && \
echo libtoolize && libtoolize --copy --automake && \
echo automake && automake --add-missing --copy

