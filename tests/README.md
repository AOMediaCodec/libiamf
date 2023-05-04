# Test vectors
Test vectors are grouped with a common prefix. For example test_000012 has five files associated with it.

- Metadata describing the test vector:`test_000012.textproto`.
- Standalone IAMF bitstream: `test_000012.iamf`.
- Fragmented MP4 file: `test_000012_f.mp4`.
- Standalone MP4 file: `test_000012_s.mp4`.
- Decoded WAV file (if present): `test_000012.wav`

## .textproto files
These file describe metadata about the test vector.

- `is_valid`: True for conformant bitstreams ("should-pass"). False for non-conformant bitstreams ("should-fail").
- `human_readable_descriptions`: A short description of what is being tested and why.
- `mp4_fixed_timestamp`: The timestamp within the MP4 file. Can be safely ignored.
- Other fields refer to the OBUs and data within the test vector.

# Input WAV files

Test vectors may have multiple substreams with several input .wav files. These .wav files may be shared with other test vectors. The .textproto file has a section which input wav file associated with each substream.
