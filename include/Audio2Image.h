#pragma once

#include <limits>
#include <string>
#include <tuple>

#include "common.h"

enum class AUDIO2IMAGE_RET_T {
  GOOD_IMPORT,
  GOOD_CONVERSION,
  GOOD_AUDIO2IMAGE,
  GOOD_FRAME_INSERTION_TO_IMAGE,

  UNFILLED_MATRIX,
  INVALID_AUDIO_FILE_TYPE,
  AUDIO_FILE_DURATION_TOO_SHORT,
  CANNOT_CLIP_AUDIO_FILE,

  FFMPEG_ERROR_OPENING_AUDIO_FILE,
  FFMPEG_ERROR_FINDING_AUDIO_STREAM_INFO,
  FFMPEG_ERROR_ALLOCATING_CODEC_CONTEXT,
  FFMPEG_ERROR_ALLOCATING_FRAME,
  FFMPEG_ERROR_RECEIVING_FRAME_FROM_CODEC,
  FFMPEG_ERROR_SENDING_PACKET_TO_CODEC,
  FFMPEG_ERROR_COPY_CODEC_PARAM_TO_CONTEXT,

  FFMPEG_CANNOT_OPEN_CODEC,
  FFMPEG_CODEC_NOT_FOUND,
  FFMPEG_AUDIO_STREAM_NOT_FOUND,
};

class Audio2Image {
 private:
  const int IMAGE_SIZE_X_PIXELS = SQUARE_IMG_SIZE_X;
  const int IMAGE_SIZE_Y_PIXELS = SQUARE_IMG_SIZE_Y;

  const int NUM_SAMPLES_PER_SEGMENT = 16;
  const int SEGMENTS_PER_FRAME = 64;

  const double AUDIO_DURATION_SEC = 10.0;

  void print_complex_fft(fftw_complex* out, int num_samples);

  std::tuple<int, int> normalize_to_pixel_values(const double frequency,
                                                 const double amplitude);

  AUDIO2IMAGE_RET_T insert_codec_frame_to_image(
      int& pixel_count,
      std::vector<float>& audio_data, cv::Mat& result_image);

  double get_audio_duration(const std::string filename);

  std::string generateClippedFilename(const std::string& filename,
                                      double new_duration);

  bool clip_audio_file(const std::string filename,
                       const std::string new_filename, double new_duration);

  std::tuple<double, double> compute_average_frequency_and_amplitude(
      fftw_complex* out, int num_samples);

 public:
  std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> audio_file_to_image(
      std::string filename);

  std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> audio2image(std::string filename);
};
