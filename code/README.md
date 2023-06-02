This library aims to be a friendly, portable C implementation of the immersive audio model and format(IAMF),
as described here:

<https://aomediacodec.github.io/iamf/>



## Usage

Please see the examples in the "test/tools" directory. If you're already building this project.

### Compiling
There are 2 parts to build: iamf(iamf_dec&iamf_enc) tools(iamfpackager&iamfplayer).

"build_x86.sh" is an example to build, you can run it directly at your side.

1. build iamf in "src" directory.
```sh
% BUILD_LIBS=$PWD/build_libs
% cmake ./ -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}
% make 
% make install
```

2. build tools in "test/tools/iamfpackager" and "test/tools/iamfplayer" directory separately
```sh
% cmake ./-DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}
% make 
```

Remark: please ensure that they have same CMAKE_INSTALL_PREFIX.


### Tools(iamfpackager)
This tool aims to encode PCM data to IA bitstream and encapsulate to Mp4/Fmp4

```sh
-profile : <0/1(simpe/base)>
-codec     : <codec name/frame size(opus,aac,flac,pcm/1024)>
-mode      : <audio element type(0:channle-based,1:scene-based(Mono),2:scene-based(Projection))/input channel layout/channel layout combinations>
-i         : <input wav file>
-o         : <0/1/2(bitstream/mp4/fmp4)> <output file>
Example:
iamfpackager -profile 0 -codec opus -mode 0/7.1.4/2.0.0+3.1.2+5.1.2 -i input.wav -o 0 simple_profile.iamf
or
iamfpackager -profile 1 -codec opus -mode 0/7.1.4/3.1.2+5.1.2 -i input1.wav -mode 1 -i input2.wav -o 0 base_profile.iamf

Before exacuting, please modify mix_config.json to set mix presentation.
```

1. encode scalable channel layout input format for simple profile.
```sh
Example:  
./iamfpackager -profile 0 -codec opus -mode 0/7.1.4/2.0.0+3.1.2+5.1.2 -i input.wav -o 0 simple_profile.iamf
```
Remark: "estimator_model.tflite" and "feature_model.tflite" are required in exacuting directory.

2. encode non-scalable channel layout input format for simple profile.
```sh
Example:  ./iamfpackager -profile 0 -codec opus -mode 0/7.1.4 -i input.wav -o 0 simple_profile.iamf
```

3. encode ambisonics input format for simple profile.
```sh
Example:
./iamfpackager -profile 0 -codec opus -mode 1 -i input.wav -o 0 simple_profile.iamf
```

4. encode for base profile.
```sh
Example:
./iamfpackager -profile 1 -codec opus -mode 0/7.1.4/3.1.2+5.1.2 -i input1.wav -mode 1 -i input2.wav -o 0 base_profile.iamf
```

### Tools(iamfplayer)
This tool aims to decode IA bitstream and dump to wav file.
```sh
./iamfplayer <options> <input file>
options:
-i[0-1]    0 : IAMF bitstream input.(default)
           1 : mp4 input.
-o2        2 : pcm output.
-r [rate]    : audio signal sampling rate, 48000 is the default.
-ts pos      : seek to a given position in seconds, which is valid when mp4 file is used as input.
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
           b : Binaural.
-p [dB]      : Peak threshold in dB.
-l [LKFS]    : Normalization loudness in LKFS.
-d [bit]     : Bit depth of pcm output.
-m           : Generate a metadata file with the suffix .met.

Example:  ./iamfplayer -o2 -s9 simple_profile.iamf
          ./iamfplayer -i1 -o2 -s9 simple_profile.mp4

```


## Build Notes

1) Building this project requires [CMake](https://cmake.org/).

2) Building this project requires opus or aac or flac library, please ensure that there are library in "dep_codecs/lib",
and there are headers in "dep_codecs/include" already. If not, please build(patch_script.sh) and install in advance.

3) "src/dmpd" part building relys on 3rd part libs("dep_external/lib": libfftw3f,libflatccrt)
They have been provided already in "dep_external/lib", if meet target link issue, please download the opensource code,
and build at your side. After building, please replace them.
[fftw](http://www.fftw.org/).
[flatcc](https://github.com/dvidelabs/flatcc).
   (Remark: please add compile options:-fPIC when compiling fftw&flatcc)



## License

Released under the BSD License.

```markdown

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
