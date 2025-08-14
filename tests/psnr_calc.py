import argparse
import wave
import os
import scipy.io.wavfile as wavfile
import numpy as np
import math

parser = argparse.ArgumentParser(description="PSNR verification script")
parser.add_argument(
    "--dir",
    type=str,
    required=True,
    help="decoder verification wav output directory",
)
parser.add_argument(
    "--target",
    type=str,
    required=True,
    help="decoder verification wav output file. Multiple files can be entered as ::. (ex - test1.wav::test2.wav)",
)
parser.add_argument(
    "--ref",
    type=str,
    required=True,
    help="decoder verification PSNR evaluation reference file. Multiple files can be entered as ::. (ex - test1.wav::test2.wav)",
)
args = parser.parse_args()


def get_sampwdith(path):
    with wave.open(path, "rb") as wf:
        sampwidth_bytes = wf.getsampwidth()
    return sampwidth_bytes


def calc_psnr(ref_signal, signal, sampwidth_bytes):
    assert (
        sampwidth_bytes > 1
    ), "Supports sample format: [pcm_s16le, pcm_s24le, pcm_s32le]"
    max_value = pow(2, sampwidth_bytes * 8) - 1

    # To prevent overflow
    ref_signal = ref_signal.astype("int64")
    signal = signal.astype("int64")

    mse = np.mean((ref_signal - signal) ** 2, axis=0, dtype="float64")

    psnr_list = list()

    # To support mono signal
    num_channels = 1 if ref_signal.shape[1:] == () else ref_signal.shape[1]
    for i in range(num_channels):
        mse_value = mse[i] if num_channels > 1 else mse
        if mse_value == 0:
            print(f"ch#{i} PSNR: inf")
        else:
            psnr_value = 10 * math.log10(max_value**2 / mse_value)
            psnr_list.append(psnr_value)
            print(f"ch#{i} PSNR: {psnr_value} dB")

    return -1 if len(psnr_list) == 0 else sum(psnr_list) / len(psnr_list)


target_file_list = args.target.split("::")
ref_file_list = args.ref.split("::")

tc_number_list = []
psnr_list = []
for file_idx in range(len(target_file_list)):
    target_file = target_file_list[file_idx]
    ref_file = ref_file_list[file_idx]
    print(
        "[%d] PSNR evaluation: compare %s with %s"
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

        ref_samplerate, ref_data = wavfile.read(ref_filepath)
        target_samplerate, target_data = wavfile.read(target_filepath)

        ref_sampwdith_bytes = get_sampwdith(ref_filepath)
        target_sampwidth_bytes = get_sampwdith(target_filepath)

        # Check sampling rate
        if not (ref_samplerate == target_samplerate):
            print(ref_file, " / ", target_file)
            raise Exception(
                "Sampling rate of reference file and comparison file are different."
            )

        # Check number of channels
        if not (ref_data.shape[1:] == target_data.shape[1:]):
            raise Exception(
                "Number of channels of reference file and comparison file are different."
            )

        # Check number of samples
        if not (ref_data.shape[0] == target_data.shape[0]):
            print(ref_file, " / ", target_file)
            raise Exception(
                "Number of samples of reference file and comparison file are different."
            )

        # Check bit depth
        if not (ref_sampwdith_bytes == target_sampwidth_bytes):
            print(ref_file, " / ", target_file)
            raise Exception(
                "Bit depth of reference file and comparison file are different."
            )

        average_psnr = calc_psnr(ref_data, target_data, ref_sampwdith_bytes)
        if average_psnr != -1:
            print("average PSNR: %.15f" % (average_psnr))
            psnr_list.append(average_psnr)
        else:
            print("average PSNR: %.15f" % (100))
            psnr_list.append(100)
    except Exception as err:
        print(str(err))
        psnr_list.append(0)
    print("")

# print result
print(
    "\n\n\n[Result] - (If the OPUS or AAC codec has a over avgPSNR 30, it is considered PASS. Other codecs must be over avgPSNR 80.)"
)
for i in range(len(tc_number_list)):
    print(
        "TC#%d : %.3f (compare %s with %s)"
        % (
            tc_number_list[i],
            round(psnr_list[i], 3),
            target_file_list[i],
            ref_file_list[i],
        )
    )
