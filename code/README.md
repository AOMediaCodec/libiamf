This library aims to be a friendly, portable C implementation of the immersive audio model and format(IAMF),
as described here:

<https://aomediacodec.github.io/iamf/>



## Usage

Please see the examples in the "test/tools" directory. If you're already building this project.

### Compiling
There are 2 parts to build: iamf(common&iamf_dec) tool(iamfplayer).

"build.sh" is an example to build, you can run it directly at your side.

1. build iamf in "src" directory.
```sh
% BUILD_LIBS=$PWD/build_libs
% cmake ./ -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS} -DBINAURALIZER=ON
% make
% make install
```

2. build tool in "test/tools/iamfplayer" directory
```sh
% cmake ./ -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS} -DBINAURALIZER=ON
% make
```

Remark: please ensure that they have same CMAKE_INSTALL_PREFIX.


### Tools(iamfplayer)
This tool aims to decode IA bitstream and dump to wav file.
```sh
./iamfplayer <options> <input file>
options:
-i[0-1]    0 : IAMF bitstream input.(default)
           1 : mp4 input.
-o2        2 : pcm output.
-r [rate]    : audio signal sampling rate, 48000 is the default.
-s[0~11,b]   : output layout, the sound system A~J and extensions (Upper + Middle + Bottom).
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
           b : Binaural.
-p [dB]      : Peak threshold in dB.
-l [LKFS]    : Normalization loudness in LKFS.
-d [bit]     : Bit depth of pcm output.
-mp [id]     : Set mix presentation id.
-m           : Generate a metadata file with the suffix .met.
-disable_limiter
             : Disable peak limiter.

Example:  ./iamfplayer -o2 -s9 simple_profile.iamf
          ./iamfplayer -i1 -o2 -s9 simple_profile.mp4

```


## Build Notes

1) Building this project requires [CMake](https://cmake.org/).

2) Building this project requires opus or aac or flac library, please ensure that there are library in "dep_codecs/lib",
and there are headers in "dep_codecs/include" already. If not, please build(patch_script.sh) and install in advance.

## License

Released under the BSD License.

```markdown

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
```

This IAMF reference software decoder uses the following open source software.
Each open source software complies with its respective license terms, and the license files
have been stored in a directory with their respective source code or library used.


```markdown

https://downloads.xiph.org/releases/opus/opus-1.4.tar.gz (/code/dep_codecs/lib/opus.license)
https://people.freedesktop.org/~wtay/fdk-aac-free-2.0.0.tar.gz (/code/dep_codecs/lib/fdk_aac.license)
https://downloads.xiph.org/releases/flac/flac-1.4.2.tar.xz (code/dep_codecs/lib/flac.license)
https://svn.xiph.org/trunk/speex/libspeex/resample.c (/code/src/iamf_dec/resample.license)
https://github.com/BelledonneCommunications/opencore-amr/blob/master/test/wavwriter.c (/code/dep_external/src/wav/dep_wavwriter.license)
```
