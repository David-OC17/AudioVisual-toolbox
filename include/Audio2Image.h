#pragma once

#include <stdint.h>

#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>

#include "common.h"

enum class AUDIO2IMAGE_RET_T {
  GOOD_IMPORT,
  GOOD_CONVERSION,
  GOOD_AUDIO2IMAGE,

  UNFILLED_MATRIX,
  INVALID_AUDIO_FILE_TYPE,
  AUDIO_FILE_DURATION_TOO_SHORT,
  CANNOT_CLIP_AUDIO_FILE,

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
  const int IMAGE_SIZE_X_PIXELS = 210;
  const int IMAGE_SIZE_Y_PIXELS = 210;
  const int NUM_SAMPLES_PER_SEGMENT =
      CD_AUDIO_FILE_FREQUENCY_HZ * 10 / (210 * 210);
  const double AUDIO_DURATION_SEC = 10.0;

  double get_audio_duration(const std::string filename);

  std::string generateClippedFilename(const std::string& filename, double new_duration);

  bool clip_audio_file(const std::string filename,
                       const std::string new_filename, double new_duration);

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
