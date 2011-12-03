#!/bin/bash

. arm-toolchain.sh

make ${arch} ${compiler} ${jobs} mx51_efikamx_defconfig
