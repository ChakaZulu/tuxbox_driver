#!/bin/bash
#
# example driver build script
# modify to suit your setup
#
# usage:
#    ./build.sh clean
#    ./build.sh
#    ./build.sh install
#
# can be called from any subdirectory
#

CDKROOT=/dbox2/cdkroot
CVSROOT=${HOME}/tuxbox-cvs
KERNEL_VERSION=2.4.20

make KERNEL_LOCATION=${CVSROOT}/cdk/linux-${KERNEL_VERSION} MODULE_DEST=${CDKROOT}/lib/modules/${KERNEL_VERSION}/misc ${1}

