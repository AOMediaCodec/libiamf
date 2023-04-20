# Test vectors
Test vectors are grouped with a common prefix. For example test_000001 has four associated files with it.

- Metdata describing the test vector:`test_000001.textproto`.
- Standalone IAMF bistream: `test_000001.iamf`.
- Framented MP4 file: `test_000001_f.mp4`.
- Standalone MP4 file: `test_000001_s.mp4`.

# .textproto files
These file describe metadata about the test vector.

- `is_valid`: True for comforant bitstreams ("should-pass"). False for nonconformant bitstreams ("should-fail").
- `human_readable_descriptions`: A short description of what is being tested and why.
- `mp4_fixed_timestamp`: The timestamp within the MP4 file. Can be safely ignored.
- Other fields refer to the OBUs and data within the test vector.

# .wav files

Test vectors may have multiple substreamd with several input .wav files. These .wav files may be shared with other test vectors. The .textproto file has a section which input wav file asssociated with each substream.
