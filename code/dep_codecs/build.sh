# Copyright (c) 2024, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
#

OPUS_DIR="$( cd "$(dirname "$0")" ; pwd -P )/opus/opus-1.4"
AAC_DIR="$( cd "$(dirname "$0")" ; pwd -P )/aac/fdk-aac-free-2.0.0"
FLAC_DIR="$( cd "$(dirname "$0")" ; pwd -P )/flac/flac-1.4.2"
CODEC_LIB_DIR="$( cd "$(dirname "$0")" ; pwd -P )/lib"

declare -a CONFIG_FLAGS_OPUS
declare -a CONFIG_FLAGS_AAC
declare -a CONFIG_FLAGS_FLAC
CONFIG_FLAGS_AAC="--enable-static --disable-shared"
CONFIG_FLAGS_FLAC="-DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF"

function show_help()
{
  cat <<EOF
*** IAMF dependent codec compile script ***

Configuration:
  -h, --help                     Display this help and exit
  --x86_64-mingw_toolchain       Use mingw-gcc X86_64 toolchain.
  --arm64-linux_toolchain        Use Linux Arm64 toolchain.
  --x86-macos_toolchain          Use MacOS X86 toolchain.
  --arm64-ios_toolchain          Use IOS Arm64 toolchain.
EOF
exit
}

ECHO=`which echo`
if test "x$ECHO" = x; then
  ECHO=echo
fi

for i in "$@"
do
  case $i in
    -help | --help | -h)
    want_help=yes ;;

    --x86_64-mingw_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86_64-mingw.cmake -DOPUS_STACK_PROTECTOR=OFF"
      CONFIG_FLAGS_AAC="--host=x86_64-w64-mingw32  --enable-static --disable-shared"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86_64-mingw.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DWITH_STACK_PROTECTOR=OFF"
      shift # past argument with no value
      ;;

    --arm64-linux_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-linux.cmake -DOPUS_STACK_PROTECTOR=OFF"
      CONFIG_FLAGS_AAC="--host=aarch64-linux-gnu  --enable-static --disable-shared"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-linux.cmake -DCMAKE_C_FLAGS="-fPIC" -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF -DWITH_STACK_PROTECTOR=OFF"
      shift # past argument with no value
      ;;

    --x86-macos_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86-macos.cmake"
      CONFIG_FLAGS_AAC="--host=x86_64-apple-darwin20.4  --enable-static --disable-shared"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/x86-macos.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF"
      shift # past argument with no value
      ;;
      
    --arm64-ios_toolchain)
      CONFIG_FLAGS_OPUS="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-ios.cmake"
      CONFIG_FLAGS_AAC="--host=aarch64-apple-darwin20.4  --enable-static --disable-shared"
      CONFIG_FLAGS_FLAC="-DCMAKE_TOOLCHAIN_FILE=../../../build/cmake/toolchains/arm64-ios.cmake -DWITH_OGG=OFF -DBUILD_CXXLIBS=OFF"
      shift # past argument with no value
      ;;

    *)
      # unknown option
      echo "Unknown option: ${i}"
      show_help
      ;;
  esac
done

if test "x$want_help" = xyes; then
show_help
fi


echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>OPUS Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $OPUS_DIR
rm -rf build
cmake -B build ./ $CONFIG_FLAGS_OPUS
cmake --build build --clean-first
cp -f build/libopus.a $CODEC_LIB_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>AAC Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $AAC_DIR
rm -rf build
mkdir build
cd build
../configure $CONFIG_FLAGS_AAC
make
cp -f .libs/libfdk-aac.a $CODEC_LIB_DIR

echo ">>>>>>>>>>>>>>>>>>>>>>>>>>>FLAC Compile<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
cd $FLAC_DIR
rm -rf build
cmake -B build ./ $CONFIG_FLAGS_FLAC
cmake --build build --clean-first
cp -f build/src/libFLAC/libFLAC.a $CODEC_LIB_DIR



