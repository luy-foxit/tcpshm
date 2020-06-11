#!/bin/bash

build=build.android
rm -rf $build && mkdir -p $build
pushd $build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_STL=c++_static \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_PLATFORM=android-27
make

