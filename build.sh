#!/bin/bash
#
# example driver build script
# modify to suit your setup
#
# usage:
#   choose one or more of the following, depending on your intention:
#
#    ./build.sh
#    ./build.sh dep
#    ./build.sh modules
#    ./build.sh modules_install
#    ./build.sh clean
#    ./build.sh distclean
#
#   view Makefile to see what the different targets do.
#

# TODO: create some kind of tuxboxcdk-config script and use it
CDKROOT=${CDKPREFIX}/dbox2/cdkroot
PATH=${CDKPREFIX}/dbox2/cdk/bin:${PATH}

CVSROOT=${HOME}/cvs/tuxbox
MAKE=/usr/bin/make

${MAKE} ${1} \
	ARCH=ppc \
	CROSS_COMPILE=powerpc-tuxbox-linux-gnu- \
	DEPMOD=/bin/true \
	KERNEL_LOCATION=${CVSROOT}/cdk/linux \
	INSTALL_MOD_PATH=${CDKROOT}

