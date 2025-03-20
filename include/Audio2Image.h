#pragma once

#include <stdint.h>
#include <string>
#include <tuple>
#include <limits>

#include "common.h"

/* Audio2Image */
enum class AUDIO2IMAGE_RET_T {
  GOOD_IMPORT,
  GOOD_CONVERSION,
  GOOD_AUDIO2IMAGE,

  UNFILLED_MATRIX,
  INVALID_AUDIO_FILE_TYPE,

  FFMPEG_ERROR_OPENING_AUDIO_FILE,
  FFMPEG_ERROR_FINDING_AUDIO_STREAM_INFO,
  FFMPEG_ERROR_ALLOCATING_CODEC_CONTEXT,
  FFMPEG_ERROR_ALLOCATING_FRAME,
  FFMPEG_ERROR_RECEIVING_FRAME,
  FFMPEG_ERROR_SENDING_PACKET_TO_DECODER,

  FFMPEG_CANNOT_OPEN_CODEC,
  FFMPEG_CODEC_NOT_FOUND,
  FFMPEG_AUDIO_STREAM_NOT_FOUND,
};

class Audio2Image {
 private:
  const int IMAGE_SIZE_X_PIXELS = 2000;
  const int IMAGE_SIZE_Y_PIXELS = 2000;

  std::tuple<double, double> compute_average_frequency_and_amplitude(
      fftw_complex* out, int num_samples);

  std::tuple<AUDIO2IMAGE_RET_T, AVFormatContext*, AVCodecContext*, AVFrame*,
             int>
  ffmpeg_import_audio_file(std::string filename);

  std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> save_average_FFT_and_amplitude(
      AVFormatContext* format_context, AVCodecContext* codec_context,
      AVFrame* frame, int num_samples, int audio_stream_index);

 public:
  std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> audio_file_to_image(
      std::string filename, uint16_t sampling_freq);
};
