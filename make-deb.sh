#!/bin/bash

. arm-toolchain.sh

target=$1

if [ "x${target}" == "x" ]; then
	target="kernel_image"
fi

packagedate=$(date +%Y.%m)
make-kpkg --uc --us --initrd --cross-compile ${toolchain} --subarch efikamx --jobs=${numjobs} --rootcmd=fakeroot --arch armel --revision ${packagedate} ${target}

