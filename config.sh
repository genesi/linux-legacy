#!/bin/bash

. toolchain.sh

make ${arch} ${compiler} ${jobs} mx51_efikamx_defconfig
