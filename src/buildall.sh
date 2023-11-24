#!/bin/bash

here=$(pwd)

cd ../circle || exit
./makeall --nosample
cd boot || exit
make install64

cd "$here" || exit
./build.sh
