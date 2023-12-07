#!/bin/bash

make
make install && \
  cp -v cmdline.txt /run/media/tar/RPI && \
  umount /run/media/tar/RPI
