#include "Audio2Image.h"

/*==========================================
=                  PUBLIC                  =
==========================================*/
std::tuple<double, double> Audio2Image::compute_average_frequency_and_amplitude(
    fftw_complex* out, int num_samples) {
  double sum_frequency = 0;
  double sum_magnitude = 0;

  for (int i = 0; i < num_samples / 2; ++i) {
    double magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
    sum_magnitude += magnitude;
    sum_frequency += i * magnitude;  // Weight frequency bins by their magnitude
  }

  double average_frequency =
      (sum_magnitude > 0) ? (sum_frequency / sum_magnitude) : 0;
  double average_amplitude =
      (num_samples > 0) ? (sum_magnitude / (num_samples / 2)) : 0;

  return {average_frequency, average_amplitude};
}

std::tuple<AUDIO2IMAGE_RET_T, AVFormatContext*, AVCodecContext*, AVFrame*, int>
Audio2Image::ffmpeg_import_audio_file(std::string filename) {
  // Init FFmpeg and open file
  av_register_all();
  AVFormatContext* format_context = nullptr;
  if (avformat_open_input(&format_context, filename.c_str(), nullptr, nullptr) <
      0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_OPENING_AUDIO_FILE, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::infinity()};
  }

  if (avformat_find_stream_info(format_context, nullptr) < 0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_FINDING_AUDIO_STREAM_INFO, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::infinity()};
  }

  // Find the audio stream
  int audio_stream_idx = -1;
  for (int i = 0; i < format_context->nb_streams; i++) {
    if (format_context->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_AUDIO) {
      audio_stream_idx = i;
      break;
    }
  }

  if (audio_stream_idx == -1) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_AUDIO_STREAM_NOT_FOUND, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::infinity()};
  }

  AVCodecContext* codec_context = avcodec_alloc_context3(nullptr);
  if (!codec_context) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_ALLOCATING_CODEC_CONTEXT, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::infinity()};
  }

  avcodec_parameters_to_context(
      codec_context, format_context->streams[audio_stream_idx]->codecpar);

  // Find the decoder for the audio stream
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
  if (!codec) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_CODEC_NOT_FOUND, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::infinity()};
  }

  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_CANNOT_OPEN_CODEC, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::infinity()};
  }

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_ALLOCATING_FRAME, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::infinity()};
  }

  return {AUDIO2IMAGE_RET_T::GOOD_IMPORT, format_context, codec_context, frame,
          audio_stream_idx};
}

std::tuple<AUDIO2IMAGE_RET_T, cv::Mat>
Audio2Image::save_average_FFT_and_amplitude(AVFormatContext* format_context,
                                            AVCodecContext* codec_context,
                                            AVFrame* frame, int num_samples,
                                            int audio_stream_index) {
  cv::Mat result_image(this->IMAGE_SIZE_X_PIXELS, this->IMAGE_SIZE_Y_PIXELS, CV_8UC3,
                       cv::Scalar(0, 0, 0));
  AVPacket packet;
  int ret;

  int x_pixel = 0;
  int y_pixel = 0;
  int pixel_count = 0;

  while (av_read_frame(format_context, &packet) >= 0) {
    if (packet.stream_index == audio_stream_index) {
      ret = avcodec_send_packet(codec_context, &packet);
      if (ret < 0) {
        av_packet_unref(&packet);
        return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_SENDING_PACKET_TO_DECODER,
                result_image};
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(codec_context, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        } else if (ret < 0) {
          return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_RECEIVING_FRAME,
                  result_image};
        }

        int channels = frame->channels;

        // Apply FFT on the section of the audio data (mono channel)
        if (channels > 0) {
          // Using the first channel for simplicity
          double* audio_data =
              reinterpret_cast<double*>(frame->extended_data[0]);

          // Apply FFT on the audioData
          fftw_complex* out =
              (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * num_samples);
          fftw_plan p =
              fftw_plan_dft_r2c_1d(num_samples, audio_data, out, FFTW_ESTIMATE);

          fftw_execute(p);

          auto [average_frequency, average_amplitude] =
              compute_average_frequency_and_amplitude(out, num_samples);

          y_pixel = pixel_count / this->IMAGE_SIZE_Y_PIXELS;
          x_pixel = pixel_count % this->IMAGE_SIZE_X_PIXELS;

          cv::Vec3b& pixel = result_image.at<cv::Vec3b>(y_pixel, x_pixel);
          pixel[0] = average_frequency;  // Blue channel
          pixel[1] = 0;                  // Green channel
          pixel[2] = average_amplitude;  // Red channel

          pixel_count++;

          // Clean up
          fftw_destroy_plan(p);
          fftw_free(out);
        }

        // Clean up the packet
        av_packet_unref(&packet);
      }
    }

    // TODO missing check?
  }

  if (pixel_count != this->IMAGE_SIZE_X_PIXELS * this->IMAGE_SIZE_Y_PIXELS) {
    return {AUDIO2IMAGE_RET_T::UNFILLED_MATRIX, result_image};
  }

  return {AUDIO2IMAGE_RET_T::GOOD_CONVERSION, result_image};
}

/*==========================================
=                 PRIVATE                  =
==========================================*/
std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> Audio2Image::audio_file_to_image(
    std::string filename, uint16_t sampling_freq) {
  // Verify audio type is valid

  // Verify that the length is correct and the number of samples needed will
  // match Interpolate if not

  // Calculate num_samples based on sampling_freq
  int num_samples = 1000;  // Number of samples per segment

  // Import audio file with FFmpeg
  auto [exit_status, format_context, codec_context, frame, audio_stream_index] =
      this->ffmpeg_import_audio_file(filename);

  if (exit_status != AUDIO2IMAGE_RET_T::GOOD_IMPORT) {
    return {exit_status, cv::Mat::zeros(0, 0, CV_8UC3)};
  }

  auto [exit_status, result_image] = this->save_average_FFT_and_amplitude(
      format_context, codec_context, frame, num_samples, audio_stream_index);

  if (exit_status != AUDIO2IMAGE_RET_T::GOOD_CONVERSION) {
    return {exit_status, cv::Mat::zeros(0, 0, CV_8UC3)};
  }

  return {AUDIO2IMAGE_RET_T::GOOD_AUDIO2IMAGE, cv::Mat::zeros(0, 0, CV_8UC3)};
}