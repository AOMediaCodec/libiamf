"""PSNR calculation utilities."""

import logging
import math
import wave
import numpy as np
import scipy.io.wavfile as wavfile


def calc_average_channel_psnr_pcm(
    ref_signal: np.ndarray, signal: np.ndarray, sampwidth_bytes: int
):
  """Calculates the PSNR between two signals.

  Args:
    ref_signal: The reference signal as a numpy array.
    signal: The signal to compare as a numpy array.
    sampwidth_bytes: The sample width in bytes (e.g. 2 for 16-bit, 3 for
      24-bit).

  Returns:
    The average PSNR in dB across all channels, or -1 if all channels are
    identical.
  """
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
      logging.debug("ch#%d PSNR: inf", i)
    else:
      psnr_value = 10 * math.log10(max_value**2 / mse_value)
      psnr_list.append(psnr_value)
      logging.debug("ch#%d PSNR: %f dB", i, psnr_value)

  return -1 if len(psnr_list) == 0 else sum(psnr_list) / len(psnr_list)


def calc_average_channel_psnr_wav(ref_filepath: str, target_filepath: str):
  """Calculates the PSNR between two WAV files.

  Args:
    ref_filepath: Path to the reference WAV file.
    target_filepath: Path to the target WAV file to compare.

  Returns:
    The average PSNR in dB across all channels. Or -1 if all channels are
    identical.

  Raises:
    Exception: If the wav files have different samplerate, channels, bit-depth
               or number of samples.
  """
  ref_wav = wave.open(ref_filepath, "rb")
  target_wav = wave.open(target_filepath, "rb")

  # Check sampling rate
  if ref_wav.getframerate() != target_wav.getframerate():
    raise ValueError(
        "Sampling rate of reference file and comparison file are different:"
        f" {ref_filepath} vs {target_filepath}"
    )

  # Check number of channels
  if ref_wav.getnchannels() != target_wav.getnchannels():
    raise ValueError(
        "Number of channels of reference file and comparison file are"
        f" different: {ref_filepath} vs {target_filepath}"
    )

  # Check number of samples
  if ref_wav.getnframes() != target_wav.getnframes():
    raise ValueError(
        "Number of samples of reference file and comparison file are different:"
        f" {ref_filepath} vs {target_filepath}"
    )

  # Check bit depth
  if ref_wav.getsampwidth() != target_wav.getsampwidth():
    raise ValueError(
        "Bit depth of reference file and comparison file are different:"
        f" {ref_filepath} vs {target_filepath}"
    )

  # Open wav as a np array
  _, ref_data = wavfile.read(ref_filepath)
  _, target_data = wavfile.read(target_filepath)

  return calc_average_channel_psnr_pcm(
      ref_data, target_data, ref_wav.getsampwidth()
  )
