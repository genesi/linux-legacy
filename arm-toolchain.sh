#!/bin/bash
archname="arm"
arch="ARCH=${archname}"
toolchain="arm-linux-gnueabi-"
compiler="CROSS_COMPILE=${toolchain}"
numjobs="16"
jobs="-j${numjobs}"
