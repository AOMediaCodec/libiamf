"""Utils for parsing user metadata."""

from dataclasses import dataclass, field
import os
import sys
import wave

sys.path.append(os.path.join(os.path.dirname(__file__), 'proto'))
import codec_config_pb2
import mix_presentation_pb2
import user_metadata_pb2


@dataclass
class TestExclusions:
  file_name_prefix: str
  mix_presentation_id: int
  layout_index: int
  reason: str


@dataclass
class TestCombinationMetadata:
  test_prefix: str
  mix_presentation_id: int
  sub_mix_index: int
  layout_index: int
  golden_wav_file_name: str
  base_name_to_generate: str
  layout_type_enum: mix_presentation_pb2.LayoutType = field(repr=False)
  sound_system_enum: mix_presentation_pb2.SoundSystem | None = field(repr=False)
  is_lossy: bool = field(repr=False)
  bit_depth: int = 16
  sample_rate: int = 48000


_SOUND_SYSTEM_TO_FLAG = {
    mix_presentation_pb2.SOUND_SYSTEM_A_0_2_0: '0',
    mix_presentation_pb2.SOUND_SYSTEM_B_0_5_0: '1',
    mix_presentation_pb2.SOUND_SYSTEM_C_2_5_0: '2',
    mix_presentation_pb2.SOUND_SYSTEM_D_4_5_0: '3',
    mix_presentation_pb2.SOUND_SYSTEM_E_4_5_1: '4',
    mix_presentation_pb2.SOUND_SYSTEM_F_3_7_0: '5',
    mix_presentation_pb2.SOUND_SYSTEM_G_4_9_0: '6',
    mix_presentation_pb2.SOUND_SYSTEM_H_9_10_3: '7',
    mix_presentation_pb2.SOUND_SYSTEM_I_0_7_0: '8',
    mix_presentation_pb2.SOUND_SYSTEM_J_4_7_0: '9',
    mix_presentation_pb2.SOUND_SYSTEM_10_2_7_0: '10',
    mix_presentation_pb2.SOUND_SYSTEM_11_2_3_0: '11',
    mix_presentation_pb2.SOUND_SYSTEM_12_0_1_0: '12',
    mix_presentation_pb2.SOUND_SYSTEM_13_6_9_0: '13',
}


def _map_layout_to_iamfdec_s_flag(
    layout_type_enum: mix_presentation_pb2.LayoutType,
    sound_system_enum: mix_presentation_pb2.SoundSystem | None,
) -> str:
  """Maps layout in metadata to '-s' flag."""
  if layout_type_enum == mix_presentation_pb2.LAYOUT_TYPE_BINAURAL:
    return 'b'
  if (
      layout_type_enum
      == mix_presentation_pb2.LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION
  ):
    flag = _SOUND_SYSTEM_TO_FLAG.get(sound_system_enum)
    if flag is None:
      # Some test vectors use reserved types.
      print(f'Could not map sound system to -s flag: {sound_system_enum}')
    return flag

  # Some test vectors use reserved types.
  print(f'Could not map layout type to -s flag: {layout_type_enum}')
  return None


def get_iamfdec_args(
    metadata: TestCombinationMetadata, input_path: str, output_path: str
) -> list[str]:
  """Generates renderer command line arguments for a TestCombinationMetadata.

  Args:
    metadata: A TestCombinationMetadata object.
    output_path: Path to output directory for generated WAV file.

  Returns:
    A list of strings for the command line arguments.
  """
  iamfdec_s_flag = _map_layout_to_iamfdec_s_flag(
      metadata.layout_type_enum, metadata.sound_system_enum
  )
  if iamfdec_s_flag is None:
    return None
  return [
      '-i0',
      '-mp',
      str(metadata.mix_presentation_id),
      f'-s{iamfdec_s_flag}',
      '-o3',
      ## Join directory and path
      os.path.join(output_path, metadata.base_name_to_generate),
      '-d',
      str(metadata.bit_depth),
      '-r',
      str(metadata.sample_rate),
      '-disable_limiter',
      os.path.join(input_path, f'{metadata.test_prefix}.iamf'),
  ]


def get_test_combination_metadata(user_metadata_proto, test_file_directory):
  """Parses TestCombinationMetadata from UserMetadata proto.

  Args:
    user_metadata_proto: A UserMetadata proto object.
    test_file_directory: Directory containing golden WAV files.

  Returns:
    A list of TestCombinationMetadata objects containing mix presentation
    layout information, or None if file_name_prefix is missing.
  """
  file_name_prefix = user_metadata_proto.test_vector_metadata.file_name_prefix
  if not file_name_prefix:
    return []
  if not user_metadata_proto.test_vector_metadata.is_valid_to_decode:
    # Skip test vectors that are not valid to decode.
    return []

  assert len(user_metadata_proto.codec_config_metadata) == 1
  codec_id = user_metadata_proto.codec_config_metadata[0].codec_config.codec_id
  is_lossy = codec_id in [
      codec_config_pb2.CODEC_ID_AAC_LC,
      codec_config_pb2.CODEC_ID_OPUS,
  ]

  result = []
  for mp_metadata in user_metadata_proto.mix_presentation_metadata:
    for sub_mix_idx, sub_mix in enumerate(mp_metadata.sub_mixes):
      for layout_idx, layout in enumerate(sub_mix.layouts):
        golden_wav_file_name = f'{file_name_prefix}_rendered_id_{mp_metadata.mix_presentation_id}_sub_mix_{sub_mix_idx}_layout_{layout_idx}.wav'
        golden_wav_path = os.path.join(
            test_file_directory, golden_wav_file_name
        )
        bit_depth = 16
        sample_rate = 48000
        if os.path.exists(golden_wav_path):
          with wave.open(golden_wav_path, 'rb') as wave_file:
            bit_depth = wave_file.getsampwidth() * 8
            sample_rate = wave_file.getframerate()
        else:
          print(
              'Warning: golden wav file not found, sometimes this is because'
              f' the mix presentation is invalid to decode: {golden_wav_path}'
          )
          continue
        ss_enum = None
        if (
            layout.loudness_layout.layout_type
            == mix_presentation_pb2.LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION
        ):
          ss_enum = layout.loudness_layout.ss_layout.sound_system
        data = TestCombinationMetadata(
            test_prefix=file_name_prefix,
            mix_presentation_id=mp_metadata.mix_presentation_id,
            sub_mix_index=sub_mix_idx,
            layout_index=layout_idx,
            golden_wav_file_name=golden_wav_file_name,
            base_name_to_generate=golden_wav_file_name.replace(
                '.wav', '_generated.wav'
            ),
            layout_type_enum=layout.loudness_layout.layout_type,
            sound_system_enum=ss_enum,
            is_lossy=is_lossy,
            bit_depth=bit_depth,
            sample_rate=sample_rate,
        )
        result.append(data)
  return result
