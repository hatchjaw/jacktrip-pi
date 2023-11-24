#!/bin/bash

make
make install && \
  umount /run/media/tar/RPI
