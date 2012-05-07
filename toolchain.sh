#!/bin/bash
archname="arm"
arch="ARCH=${archname}"
toolchain="arm-linux-gnueabi-"
compiler="CROSS_COMPILE=${toolchain}"
compilerversion="4.4"
numjobs="16"
jobs="-j${numjobs}"
