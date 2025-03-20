#include "Audio2Image.h"

/*==========================================
=                 PRIVATE                  =
==========================================*/

double Audio2Image::get_audio_duration(const std::string filename) {
  if (!(std::regex_match(filename, CDAudioFormatsRegex::WAV) ||
        std::regex_match(filename, CDAudioFormatsRegex::AIFF) ||
        std::regex_match(filename, CDAudioFormatsRegex::FLAC))) {
    return -1.0;  // No valid audio file format
  }

  std::string command =
      "ffprobe -i \"" + filename +
      "\" -show_entries format=duration -v quiet -of csv=\"p=0\"";
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) return -1;

  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);

  return std::stod(result);
}

std::string Audio2Image::generateClippedFilename(const std::string& filename,
                                                 double new_duration) {
  size_t lastDot = filename.find_last_of(".");
  if (lastDot == std::string::npos) {
    return filename + "-" + std::to_string((int)new_duration) +
           "sec";  // No extension case
  }

  std::string namePart = filename.substr(0, lastDot);  // Without extension
  std::string extPart = filename.substr(lastDot);  // Extension (including dot)

  return namePart + "-" + std::to_string((int)new_duration) + "sec" + extPart;
}

bool Audio2Image::clip_audio_file(const std::string filename,
                                  const std::string new_filename,
                                  double new_duration) {
  if (new_duration <= 0) {
    std::cerr << "Invalid duration: " << new_duration << " seconds\n";
    return false;
  }

  std::ostringstream command;
  command << "ffmpeg -i \"" << filename << "\" -t " << new_duration
          << " -c copy \"" << new_filename << "\" -y 2>&1";

  FILE* pipe = popen(command.str().c_str(), "r");
  if (!pipe) {
    std::cerr << "Failed to run FFmpeg command\n";
    return false;
  }

  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  int exit_code = pclose(pipe);

  if (exit_code == 0) {
    std::cout << "Audio successfully clipped to " << new_duration
              << " seconds: " << new_filename << std::endl;
    return true;
  } else {
    std::cerr << "Error occurred during audio clipping\n";
    return false;
  }
}

std::tuple<double, double> Audio2Image::compute_average_frequency_and_amplitude(
    fftw_complex* out, int num_samples) {
  double sum_frequency = 0;
  double sum_magnitude = 0;

  // Loop through the first half of the FFT output (symmetric for real signals)
  for (int i = 0; i < num_samples / 2; ++i) {
    // Compute magnitude of the complex frequency component
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
  av_register_all();
  AVFormatContext* format_context = nullptr;

  // Open the specified audio file
  if (avformat_open_input(&format_context, filename.c_str(), nullptr, nullptr) <
      0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_OPENING_AUDIO_FILE, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::max()};
  }

  // Retrieve and parse stream information from the file
  if (avformat_find_stream_info(format_context, nullptr) < 0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_FINDING_AUDIO_STREAM_INFO, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::max()};
  }

  // Identify the audio stream within the file
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
            nullptr, std::numeric_limits<int>::max()};
  }

  // Allocate a codec context for the audio stream
  AVCodecContext* codec_context = avcodec_alloc_context3(nullptr);
  if (!codec_context) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_ALLOCATING_CODEC_CONTEXT, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::max()};
  }

  // Copy codec parameters from stream to codec context
  if (avcodec_parameters_to_context(
          codec_context, format_context->streams[audio_stream_idx]->codecpar) <
      0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_ALLOCATING_CODEC_CONTEXT, nullptr,
            nullptr, nullptr, std::numeric_limits<int>::max()};
  }

  // Find an appropriate decoder for the audio stream
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
  if (!codec) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_CODEC_NOT_FOUND, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::max()};
  }

  // Open the codec for decoding
  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_CANNOT_OPEN_CODEC, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::max()};
  }

  // Allocate a frame to store decoded audio data
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_ALLOCATING_FRAME, nullptr, nullptr,
            nullptr, std::numeric_limits<int>::max()};
  }

  return {AUDIO2IMAGE_RET_T::GOOD_IMPORT, format_context, codec_context, frame,
          audio_stream_idx};
}

std::tuple<AUDIO2IMAGE_RET_T, cv::Mat>
Audio2Image::save_average_FFT_and_amplitude(AVFormatContext* format_context,
                                            AVCodecContext* codec_context,
                                            AVFrame* frame, int num_samples,
                                            int audio_stream_index) {
  cv::Mat result_image(this->IMAGE_SIZE_X_PIXELS, this->IMAGE_SIZE_Y_PIXELS,
                       CV_8UC3, cv::Scalar(0, 0, 0));
  AVPacket packet;
  int ret;
  int x_pixel = 0, y_pixel = 0, pixel_count = 0;

  // Read packets from the audio stream
  while (av_read_frame(format_context, &packet) >= 0) {
    if (packet.stream_index == audio_stream_index) {
      // Send the packet to the decoder
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
          av_packet_unref(&packet);
          return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_RECEIVING_FRAME,
                  result_image};
        }

        int channels = frame->channels;
        if (channels > 0) {
          uint8_t* raw_audio_data = frame->extended_data[0];
          double* audio_data = reinterpret_cast<double*>(raw_audio_data);

          // Perform FFT on audio data
          fftw_complex* out =
              (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * num_samples);
          fftw_plan p =
              fftw_plan_dft_r2c_1d(num_samples, audio_data, out, FFTW_ESTIMATE);
          fftw_execute(p);

          auto [average_frequency, average_amplitude] =
              compute_average_frequency_and_amplitude(out, num_samples);

          // Compute pixel coordinates
          y_pixel = pixel_count / this->IMAGE_SIZE_Y_PIXELS;
          x_pixel = pixel_count % this->IMAGE_SIZE_X_PIXELS;

          // Check pixel are in-bounds
          if (y_pixel >= this->IMAGE_SIZE_Y_PIXELS ||
              x_pixel >= this->IMAGE_SIZE_X_PIXELS) {
            fftw_destroy_plan(p);
            fftw_free(out);
            return {AUDIO2IMAGE_RET_T::UNFILLED_MATRIX, result_image};
          }

          // Store computed values in the image matrix
          cv::Vec3b& pixel = result_image.at<cv::Vec3b>(y_pixel, x_pixel);
          pixel[0] = average_frequency;  // Blue channel
          pixel[1] = 0;                  // Green channel
          pixel[2] = average_amplitude;  // Red channel
          pixel_count++;

          // Clean up
          fftw_destroy_plan(p);
          fftw_free(out);
        }
        av_packet_unref(&packet);
      }
    }
    av_packet_unref(&packet);
  }

  return (pixel_count == this->IMAGE_SIZE_X_PIXELS * this->IMAGE_SIZE_Y_PIXELS)
             ? std::make_tuple(AUDIO2IMAGE_RET_T::GOOD_CONVERSION, result_image)
             : std::make_tuple(AUDIO2IMAGE_RET_T::UNFILLED_MATRIX,
                               result_image);
}

/*==========================================
=                  PUBLIC                  =
==========================================*/

std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> Audio2Image::audio_file_to_image(
    std::string filename, uint16_t sampling_freq) {
  // Verify audio type is valid
  if (!(std::regex_match(filename, CDAudioFormatsRegex::WAV) ||
        std::regex_match(filename, CDAudioFormatsRegex::AIFF) ||
        std::regex_match(filename, CDAudioFormatsRegex::FLAC))) {
    return {AUDIO2IMAGE_RET_T::INVALID_AUDIO_FILE_TYPE,
            cv::Mat::zeros(0, 0, CV_8UC3)};
  }

  if (this->get_audio_duration(filename) < 10.0) {
    return {AUDIO2IMAGE_RET_T::AUDIO_FILE_DURATION_TOO_SHORT,
            cv::Mat::zeros(0, 0, CV_8UC3)};
  } else if (this->get_audio_duration(filename) > 10.0) {
    std::string new_filename = this->generateClippedFilename(
        filename, this->AUDIO_DURATION_SEC);  // filename-10sec.format
    if (!this->clip_audio_file(filename, new_filename,
                               this->AUDIO_DURATION_SEC)) {
      return {AUDIO2IMAGE_RET_T::CANNOT_CLIP_AUDIO_FILE,
              cv::Mat::zeros(0, 0, CV_8UC3)};
    }
    filename = new_filename;
  }

  // Import audio file with FFmpeg
  auto [exit_status, format_context, codec_context, frame, audio_stream_index] =
      this->ffmpeg_import_audio_file(filename);

  if (exit_status != AUDIO2IMAGE_RET_T::GOOD_IMPORT) {
    return {exit_status, cv::Mat::zeros(0, 0, CV_8UC3)};
  }

  auto [exit_status, result_image] = this->save_average_FFT_and_amplitude(
      format_context, codec_context, frame, this->NUM_SAMPLES_PER_SEGMENT,
      audio_stream_index);

  if (exit_status != AUDIO2IMAGE_RET_T::GOOD_CONVERSION) {
    return {exit_status, cv::Mat::zeros(0, 0, CV_8UC3)};
  }

  return {AUDIO2IMAGE_RET_T::GOOD_AUDIO2IMAGE, cv::Mat::zeros(0, 0, CV_8UC3)};
}