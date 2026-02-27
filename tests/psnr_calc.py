import argparse
import logging
import os
import dsp_utils


def main():
  """Main function for PSNR calculation script."""
  parser = argparse.ArgumentParser(description='PSNR verification script')
  parser.add_argument(
      '--dir',
      type=str,
      required=True,
      help='decoder verification wav output directory',
  )
  parser.add_argument(
      '--target',
      type=str,
      required=True,
      help=(
          'decoder verification wav output file. Multiple files can be entered'
          ' as ::. (ex - test1.wav::test2.wav)'
      ),
  )
  parser.add_argument(
      '--ref',
      type=str,
      required=True,
      help=(
          'decoder verification PSNR evaluation reference file. Multiple files'
          ' can be entered as ::. (ex - test1.wav::test2.wav)'
      ),
  )
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Verbose logging, of PSNR valuesfor each channel.',
  )
  args = parser.parse_args()
  logging.basicConfig(
      level=logging.DEBUG if args.verbose_logging else logging.INFO,
      format='%(message)s',
  )

  target_file_list = args.target.split('::')
  ref_file_list = args.ref.split('::')

  tc_number_list = []
  psnr_list = []
  for file_idx in range(len(target_file_list)):
    target_file = target_file_list[file_idx]
    ref_file = ref_file_list[file_idx]
    print(
        '[%d] PSNR evaluation: compare %s with %s'
        % (file_idx, target_file, ref_file)
    )
    tc_number_list.append(file_idx)
    try:
      ref_filepath = os.path.join(
          os.path.dirname(os.path.abspath(__file__)), args.dir, ref_file
      )
      target_filepath = os.path.join(
          os.path.dirname(os.path.abspath(__file__)), args.dir, target_file
      )

      average_psnr = dsp_utils.calc_average_channel_psnr_wav(
          ref_filepath, target_filepath
      )
      if average_psnr != -1:
        print('average PSNR: %.15f' % (average_psnr))
        psnr_list.append(average_psnr)
      else:
        print('average PSNR: %.15f' % (100))
        psnr_list.append(100)
    except ValueError as err:
      print(str(err))
      psnr_list.append(0)
    print('')

  # print result
  print(
      '\n\n\n[Result] - (If the OPUS or AAC codec has a over avgPSNR 30, it is'
      ' considered PASS. Other codecs must be over avgPSNR 80.)'
  )
  for i in range(len(tc_number_list)):
    print(
        'TC#%d : %.3f (compare %s with %s)'
        % (
            tc_number_list[i],
            round(psnr_list[i], 3),
            target_file_list[i],
            ref_file_list[i],
        )
    )


if __name__ == '__main__':
  main()
