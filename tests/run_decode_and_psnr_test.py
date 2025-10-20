import argparse
from collections import defaultdict
import csv
from dataclasses import dataclass, field
import enum
import glob
import logging
import os
import re
import subprocess
import sys
from typing import Optional
from google.protobuf import text_format
from tqdm import tqdm

sys.path.append(os.path.join(os.path.dirname(__file__), 'proto'))
import user_metadata_pb2
import dsp_utils
import user_metadata_parsing_utils as utils
from user_metadata_parsing_utils import TestExclusions


exclusions = [
    TestExclusions(
        file_name_prefix='test_000710',
        mix_presentation_id=42,
        layout_index=0,
        reason='Mix surpasses base-enhanced profile limits',
    ),
    TestExclusions(
        file_name_prefix='test_000711',
        mix_presentation_id=42,
        layout_index=0,
        reason='Mix surpasses base-enhanced profile limits',
    ),
    TestExclusions(
        file_name_prefix='test_000126',
        mix_presentation_id=42,
        layout_index=1,
        reason='Extension layouts cannot be decoded.',
    ),
]

# Opus/AAC are lossy codecs, we allow a more lenient threshold for them.
LOSSY_PSNR_THRESHOLD = 30
LOSSLESS_PSNR_THRESHOLD = 80


class ResultStatus(enum.Enum):
  SUCCESS = 1
  FAILURE = 2
  CRASH = 3
  SKIPPED = 4


@dataclass
class Result:
  test_prefix: str
  is_lossy: bool
  mix_presentation_id: int
  sub_mix_index: int
  layout_index: int
  psnr_score: Optional[float] = None
  reason: Optional[str] = None
  iamfdec_command: Optional[str] = None

  def log(self, status: ResultStatus):
    logging.debug(
        '%s: %s >= %s for %s',
        status.name,
        self.psnr_score,
        self.is_lossy,
        self.test_prefix,
    )
    logging.debug('')


@dataclass
class TestSummary:
  results: dict[ResultStatus, list[Result]] = field(
      default_factory=lambda: defaultdict(list)
  )

  def print_test_summary(self, csv_summary_file=None):
    """Prints test summary to console and optionally a CSV file.

    Args:
      csv_summary_file: Path to CSV file to log test results.
    """
    logging.info('\n-----------------SUMMARY-----------------')
    for status in ResultStatus:
      logging.info('%s: %d', status.name, len(self.results[status]))

    logging.info('-----------------------------------------')

    if csv_summary_file:
      with open(csv_summary_file, 'w', newline='') as csvfile:
        csvwriter = csv.writer(csvfile)
        csvwriter.writerow([
            'Test Prefix',
            'Mix ID',
            'Submix Index',
            'Layout Index',
            'Status',
            'PSNR',
            'Is Lossy',
            'Reason',
            'Command',
        ])
        for status in ResultStatus:
          for item in self.results[status]:
            csvwriter.writerow([
                item.test_prefix,
                item.mix_presentation_id,
                item.sub_mix_index,
                item.layout_index,
                status.name,
                item.psnr_score if item.psnr_score is not None else '',
                "lossy" if item.is_lossy else 'lossless',
                item.reason if item.reason is not None else '',
                item.iamfdec_command
                if item.iamfdec_command is not None
                else '',
            ])


def run_decoder(args, metadata):
  """Runs the iamfdec decoder and returns True if successful."""
  iamfdec_args = utils.get_iamfdec_args(
      metadata, args.test_file_directory, args.working_directory
  )
  if iamfdec_args is None:
    return False, None
  cmd = [args.iamfdec_path] + iamfdec_args
  cmd_str = ' '.join(cmd)

  verbose_logging = logging.getLogger().isEnabledFor(logging.DEBUG)
  logging.debug('Running: %s', cmd)
  try:
    subprocess.run(
        cmd,
        check=True,
        stdout=subprocess.PIPE if verbose_logging else subprocess.DEVNULL,
        stderr=subprocess.PIPE if verbose_logging else subprocess.DEVNULL,
    )
  except subprocess.CalledProcessError:
    return False, cmd_str
  return True, cmd_str


def run_psnr_test(args, metadata):
  """Gets PSNR score, returns None if calculation fails.

  Args:
    args: Command line arguments.
    metadata: Metadata for the test vector.

  Returns:
    A tuple of (ResultStatus, reason, psnr_score).
  """
  ref_file = os.path.join(
      args.test_file_directory, metadata.golden_wav_file_name
  )
  test_file = os.path.join(
      args.working_directory, metadata.base_name_to_generate
  )
  assert os.path.exists(ref_file), f'Reference file {ref_file} does not exist.'
  assert os.path.exists(test_file), f'Test file {test_file} does not exist.'
  logging.debug('ref_file: %s', ref_file)
  logging.debug('test_file: %s', test_file)
  try:
    raw_psnr_score = dsp_utils.calc_average_channel_psnr_wav(
        ref_file, test_file
    )
  except ValueError as e:
    print(f'Failed to calculate PSNR: {e}')
    return ResultStatus.CRASH, 'PSNR calculation failed', None

  psnr_score = 100 if raw_psnr_score == -1 else raw_psnr_score
  # Check if this PSNR is a pass or a fail, it depends on whether the test
  # represents a lossy or lossless codec.
  logging.debug('psnr score: %s', psnr_score)
  threshold = (
      LOSSY_PSNR_THRESHOLD if metadata.is_lossy else LOSSLESS_PSNR_THRESHOLD
  )
  if psnr_score >= threshold:
    return ResultStatus.SUCCESS, None, psnr_score
  else:
    return ResultStatus.FAILURE, 'PSNR score below threshold.', psnr_score


def _is_excluded(metadata, exclusions):
  for exclusion in exclusions:
    if (
        exclusion.file_name_prefix == metadata.test_prefix
        and exclusion.mix_presentation_id == metadata.mix_presentation_id
        and exclusion.layout_index == metadata.layout_index
    ):
      print(
          f'Skipping {metadata.test_prefix} layout'
          f' {metadata.layout_index} for mix ID'
          f' {metadata.mix_presentation_id} because ({exclusion.reason})'
      )
      return exclusion.reason
  return None


def run_tests(args, text_proto_files) -> TestSummary:
  """Runs tests on the given textproto files.

  Args:
    args: Command line arguments.
    text_proto_files: List of textproto files to run tests on.

  Returns:
    A TestSummary object containing the results of the tests.
  """
  summary = TestSummary()
  progress_bar = tqdm(text_proto_files)
  for text_proto_path in progress_bar:
    progress_bar.set_description(os.path.basename(text_proto_path))
    with open(text_proto_path, 'r') as f:
      user_metadata = text_format.Parse(
          f.read(), user_metadata_pb2.UserMetadata()
      )

    # Get the metadata for this test vector, there may be multiple mix
    # presentation, submix, and layout combinations per test vector.
    metadatas = utils.get_test_combination_metadata(
        user_metadata, args.test_file_directory
    )
    for metadata in metadatas:
      # Usually each loop will generate a new file. Delete them if they are new
      file_to_generate = os.path.join(
          args.working_directory, metadata.base_name_to_generate
      )
      generated_file_is_new = not os.path.exists(file_to_generate)
      cleanup_after_decode = (
          generated_file_is_new and not args.preserve_output_files
      )
      status, reason, psnr_score, cmd_str = None, None, None, None
      if skip_reason := _is_excluded(metadata, exclusions):
        # Test was intentionally excluded.
        reason = skip_reason
        status = ResultStatus.SKIPPED
      else:
        decoder_success, cmd_str = run_decoder(args, metadata)
        if decoder_success:
          # Run the PSNR test, this could crash, or be better than or worse than
          # the threshold PSNR.
          status, reason, psnr_score = run_psnr_test(args, metadata)
        else:
          # Decoder crashed.
          reason = 'iamfdec crash'
          status = ResultStatus.CRASH

      # Regardless of status, record what happened.
      result = Result(
          metadata.test_prefix,
          metadata.is_lossy,
          metadata.mix_presentation_id,
          metadata.sub_mix_index,
          metadata.layout_index,
          psnr_score,
          reason,
          iamfdec_command=cmd_str,
      )
      result.log(status)
      summary.results[status].append(result)
      if cleanup_after_decode and os.path.exists(file_to_generate):
        os.remove(file_to_generate)
  return summary


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-t', '--test_file_directory', required=True)
  parser.add_argument('-w', '--working_directory', required=True)
  parser.add_argument('-l', '--iamfdec_path', required=True)
  parser.add_argument(
      '-p',
      '--preserve_output_files',
      action='store_true',
      help='Preserve output files in working directory.',
  )
  parser.add_argument(
      '-r',
      '--regex_filter',
      help='Regex filter to apply to textproto filenames.',
  )
  parser.add_argument(
      '-v',
      '--verbose_test_summary',
      action='store_true',
      help='Print verbose test summary',
  )
  parser.add_argument(
      '-c',
      '--csv_summary_file',
      help='Path to CSV file to log test results.',
  )
  args = parser.parse_args()

  logging.basicConfig(
      level=logging.DEBUG if args.verbose_test_summary else logging.INFO,
      format='%(message)s',
  )

  # The textprotos contain metadata the various test vectors.
  glob_path = os.path.join(args.test_file_directory, '*.textproto')
  text_proto_files = glob.glob(glob_path)
  assert text_proto_files, f'No textproto files found in {glob_path}'

  if args.regex_filter:
    text_proto_files = [
        f
        for f in text_proto_files
        if re.search(args.regex_filter, os.path.basename(f))
    ]
  text_proto_files.sort()

  test_summary = run_tests(args, text_proto_files)
  test_summary.print_test_summary(args.csv_summary_file)


if __name__ == '__main__':
  main()
