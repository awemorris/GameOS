#!/bin/sh

set -eu

cd bootloader/boot98
make
cd ../..

cd build-pc98
make
cd ..

cat bootloader/boot98/obj/bootsect.bin build-pc98/kernel > bootdisk.raw
python3 scripts/makehdi.py

cp bootdisk.hdi ~/work/pc98/
