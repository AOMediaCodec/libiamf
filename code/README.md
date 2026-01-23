# [IAMF](https://aomediacodec.github.io/iamf/) Library

## Building the library

### Prerequisites
 1. [CMake](https://cmake.org) version 3.28 or higher.
 2. [Git](https://git-scm.com/).

### Get the code

The IAMF library source code is stored in the Alliance for Open Media Git
repository:

~~~
    $ git clone https://github.com/AOMediaCodec/libiamf
    $ git submodule update --init --recursive
    # By default, the above command stores the source in the libiamf/code directory:
    $ cd libiamf/code
~~~

### Basic build

~~~
    $ cmake -B build
    $ cmake --build build
~~~

### Configuration options

The IAMF library has few configuration options.  
1. Build binaural rendering options. These have the form `IAMF_ENABLE_BINAURALIZER`.
~~~
    $ cmake -B build -DIAMF_ENABLE_BINAURALIZER=OFF
~~~
2. Build dependent codec libraries options. These have the form `ENABLE_BUILD_CODECS`.  
The dependent codec libraries are downloaded and built by default, if they are installed in `code/dep_codecs` already, this option can be used to disable building again.  
~~~
    $ cmake -B build -DENABLE_BUILD_CODECS=OFF
~~~

### Dylib builds

A dylib (shared object) build of the IAMF library is enabled by default.
~~~
    $ cmake -B build
    $ cmake --build build
~~~

### Microsoft Visual Studio builds

Building the IAMF library in Microsoft Visual Studio is supported.  
The following example demonstrates generating projects and a solution for the Microsoft IDE:
~~~
    # This does not require a bash shell; Command Prompt (cmd.exe) is fine.
    # This assumes the build host is a Windows x64 computer.

    # To create a Visual Studio 2022 solution for the x64 target:
    $ cmake -B build -DIAMF_TEST_TOOL=ON -DIAMF_BUILD_SHARED_LIB=OFF -G "Visual Studio 17 2022"
    $ cmake --build build
~~~

### Cross compiling

CMake build handles cross compiling via the use of toolchain files  
The following example demonstrates use of an available x86-macos.cmake toolchain file on the x86_64 MacOS host:
~~~
    $ cmake -B build -DCMAKE_TOOLCHAIN_FILE=x86-macos.cmake
    $ cmake --build build
~~~

## Testing the IAMF

The iamfdec is a test application to decode an IAMF bitstream or mp4 file with IAMF encapsulation.

Please add the compile option `-DIAMF_TEST_TOOL=ON` if you wish to use this tool.

### iamfdec
This tool aims to decode IA bitstream and dump to wav file.

```sh
./iamfdec <options> <input file>
options:
-i[0-1]    0 : IAMF bitstream input.(default)
           1 : MP4 input.
-o[1-3]    1 : Output IAMF stream info.
           2 : WAVE output, same path as binary.(default)
           3 [path]
             : WAVE output, user setting path.
-r [rate]    : Audio signal sampling rate, 48000 is the default.
-s[0~14,b]   : Output layout, the sound system A~J and extensions (Upper + Middle + Bottom).
           0 : Sound system A (0+2+0)
           1 : Sound system B (0+5+0)
           2 : Sound system C (2+5+0)
           3 : Sound system D (4+5+0)
           4 : Sound system E (4+5+1)
           5 : Sound system F (3+7+0)
           6 : Sound system G (4+9+0)
           7 : Sound system H (9+10+3)
           8 : Sound system I (0+7+0)
           9 : Sound system J (4+7+0)
          10 : Sound system extension 712 (2+7+0)
          11 : Sound system extension 312 (2+3+0)
          12 : Sound system mono (0+1+0)
          13 : Sound system extension 916 (6+9+0)
          14 : Sound system extension 7154 (5+7+4)
           b : Binaural.
-p [dB]      : Peak threshold in dB.
-l [LKFS]    : Normalization loudness(<0) in LKFS.
-d [bits]    : Bit depth of pcm output.
-mp [id]     : Set mix presentation id.
-disable_limiter
             : Disable peak limiter.
-profile [n] : Set IAMF profile (0=SIMPLE, 1=BASE, 2=BASE_ENHANCED, 3=BASE_ADVANCED, 4=ADVANCED_1, 5=ADVANCED_2).
-ego id1:gain1,id2:gain2,...
             : Set element gain offsets for multiple audio elements.

Example:  ./iamfdec -o2 -s9 simple_profile.iamf
          ./iamfdec -i1 -o2 -s9 simple_profile.mp4
          ./iamfdec -i1 -o3 out.wav -s9 simple_profile.mp4

```

### Channel ordering

The ordering of channel from IAMF decoder is based on the related [ITU-2051-3](https://www.itu.int/rec/R-REC-BS.2051) layout.  
Please refer to the [Output WAV files](https://github.com/AOMediaCodec/libiamf/blob/main/tests/README.md#output-wav-files) in `test` directory, or liboar's [loudspeaker ordering](https://github.com/AOMediaCodec/oar-private/blob/main/liboar/doc/loudspeaker_layout_channel_order.md) documentation

## Coding style

We are using the Google C Coding Style defined by the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

The coding style used by this project is enforced with clang-format using the
configuration contained in the
[.clang-format](.clang-format)
file in the root of the repository.

Before pushing changes for review you can format your code with:

~~~
    # Apply clang-format to modified .c, .h files
    $ clang-format -i --style=file \
      $(git diff --name-only --diff-filter=ACMR '*.[hc]')
~~~

## Bug reports

Bug reports can be filed in the Alliance for Open Media
[issue tracker](https://github.com/AOMediaCodec/libiamf/issues/list).

## License

~~~
    BSD 3-Clause Clear License The Clear BSD License

    Copyright (c) 2022, Alliance for Open Media

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted (subject to the limitations in the disclaimer below) provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the distribution.

    3. Neither the name of the Alliance for Open Media nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.


    NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
    THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
~~~

This IAMF reference software decoder uses the following open source software.
Each open source software complies with its respective license terms, and the license files
have been stored in a directory with their respective source code or library used. The open 
source software listed below is not considered to be part of the IAMF Final Deliverable.

~~~
    https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz (code/dep_codecs/lib/opus.license)
    https://people.freedesktop.org/~wtay/fdk-aac-free-2.0.0.tar.gz (code/dep_codecs/lib/fdk_aac.license)
    https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz (code/dep_codecs/lib/flac.license)
    https://svn.xiph.org/trunk/speex/libspeex/resample.c (code/src/iamf_dec/resample.license)
    https://github.com/BelledonneCommunications/opencore-amr/blob/master/test/wavwriter.c (code/dep_external/src/wav/dep_wavwriter.license)
~~~
