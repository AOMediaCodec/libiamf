"""PSNR calculation utilities."""

import logging
import math
import wave
import numpy as np
import scipy.io.wavfile as wavfile
import librosa


def calc_per_channel_psnr_pcm(
    ref_signal: np.ndarray, signal: np.ndarray, sampwidth_bytes: int
):
  """Calculates the PSNR between two signals.

  Args:
    ref_signal: The reference signal as a numpy array.
    signal: The signal to compare as a numpy array.
    sampwidth_bytes: The sample width in bytes (e.g. 2 for 16-bit, 3 for
      24-bit).

  Returns:
    The per channel PSNR in dB.
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
      psnr_list.append(np.inf)
      logging.debug("ch#%d PSNR: inf", i)
    else:
      psnr_value = 10 * math.log10(max_value**2 / mse_value)
      psnr_list.append(psnr_value)
      logging.debug("ch#%d PSNR: %f dB", i, psnr_value)

  return psnr_list


def calc_per_channel_lsd_pcm(ref_signal: np.ndarray,
                             signal: np.ndarray,
                             sampling_rate: int):
  """Calculates the log spectral distance using Mel bins between two signals.

  Args:
    ref_signal: The reference signal as a numpy array.
    signal: The signal to compare as a numpy array.
    sampling rate: The sampling rate of the signals in Hz.

  Returns:
    The per channel log spectral distance in dB.
  """
  eps = 1e-4

  # Convert to float
  ref_signal = ref_signal / np.iinfo(ref_signal.dtype).max
  signal = signal / np.iinfo(signal.dtype).max

  lsd_list = list()

  # To support mono channel
  num_channels = 1 if ref_signal.shape[1:] == () else ref_signal.shape[1]
  for i in range(num_channels):
    ref_channel = ref_signal[:, i] if num_channels > 1 else ref_signal
    signal_channel = signal[:, i] if num_channels > 1 else signal

    lsd_frames = list()

    # Compute mel spectrogram
    mel_ref = librosa.feature.melspectrogram(y=ref_channel, sr=sampling_rate)
    mel_signal = librosa.feature.melspectrogram(y=signal_channel,
                                                sr=sampling_rate)

    log_mel_ref = 10 * np.log10(mel_ref + eps)
    log_mel_signal = 10 * np.log10(mel_signal + eps)

    diff_squared = (log_mel_ref - log_mel_signal) ** 2

    # Average across mel bins, which is the 0th dimension
    lsd_per_frame = np.sqrt(np.mean(diff_squared, axis=0))

    # shape: (1, num_frames) -> (num_frames,)
    lsd_per_frame = np.squeeze(lsd_per_frame)

    lsd_value = np.mean(lsd_per_frame)
    lsd_list.append(lsd_value)
    logging.debug('ch#d LSD: %f dB', i, lsd_value)

  return lsd_list


def calc_score_wav(ref_filepath: str, target_filepath: str, metric: str):
  """Calculates the score between two WAV files.

  Args:
    ref_filepath: Path to the reference WAV file.
    target_filepath: Path to the target WAV file to compare.
    metric: one of 'PSNR' or 'SNR'.

  Returns:
    The score in dB, averaged over all channels.

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

  if metric == 'PSNR':
    scores_list = calc_per_channel_psnr_pcm(
          ref_data, target_data, ref_wav.getsampwidth()
    )
  elif metric == 'LSD':
    scores_list = calc_per_channel_lsd_pcm(ref_data, target_data,
                                           ref_wav.getframerate())
  else:
    return None

  return np.mean(scores_list)
