#!/bin/sh

set -eu

cd bootloader/boot86
make
cd ../..

cd build-pcat
make
cd ..

cat bootloader/boot86/obj/bootsect.bin build-pcat/kernel > pcat.raw


qemu-system-i386 -m 256 -hda pcat.raw
