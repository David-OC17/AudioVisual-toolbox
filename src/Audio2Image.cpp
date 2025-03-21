#include "Audio2Image.h"

/*==========================================
=                 PRIVATE                  =
==========================================*/

void Audio2Image::print_complex_fft(fftw_complex* out, int num_samples) {
  std::vector<std::complex<double>> fft_result(num_samples);
  for (int i = 0; i < num_samples; ++i) {
    fft_result[i] = std::complex<double>(out[i][0], out[i][1]);
  }

  for (const auto& cplx : fft_result) {
    std::cout << "Real: " << cplx.real() << " Imag: " << cplx.imag()
              << std::endl;
  }
}

std::tuple<int, int> Audio2Image::normalize_to_pixel_values(
    const double frequency, const double amplitude) {
  if (amplitude < -1.0 || amplitude > 1.0) {
    LOG_WARNING(std::format("Invalid amplitude value: {}", amplitude));
    return std::make_tuple(std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::min());
  }

  int frequency_value = static_cast<int>(
      (frequency / MAX_FREQUENCY_IN_AUDIO_SIGNAL_HZ) * MAX_PIXEL_VALUE);

  // Normalize amplitude to a 0-255 scale using a logarithmic scale
  double normalized_amplitude = std::clamp(amplitude, -1.0, 1.0);
  double log_amplitude =
      std::log10(std::abs(normalized_amplitude) +
                 1e-10);  // Prevent log(0) by adding a small epsilon

  // Map the logarithmic value to the 0-255 scale
  int amplitude_value =
      static_cast<int>((log_amplitude * MAX_PIXEL_VALUE) /
                       std::log10(2.0));  // Scale based on log10(2)

  amplitude_value = std::clamp(amplitude_value, 0, 255);
  frequency_value = std::clamp(frequency_value, 0, 255);

  return std::make_tuple(frequency_value, amplitude_value);
}

AUDIO2IMAGE_RET_T Audio2Image::insert_codec_frame_to_image(
    int& pixel_count,
    std::vector<float>& audio_data, cv::Mat& result_image) {
  // Create an FFT plan for a single segment
  fftw_complex* in = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) *
                                                this->NUM_SAMPLES_PER_SEGMENT);
  fftw_complex* out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) *
                                                 this->NUM_SAMPLES_PER_SEGMENT);
  fftw_plan p = fftw_plan_dft_1d(this->NUM_SAMPLES_PER_SEGMENT, in, out, FFTW_FORWARD,
                                 FFTW_ESTIMATE);

  // Process the FFT for each window (segment)
  for (int frame_sec_increment = 0;
       frame_sec_increment < this->SEGMENTS_PER_FRAME; ++frame_sec_increment) {
    int window_start = frame_sec_increment * NUM_SAMPLES_PER_SEGMENT;

    if (window_start + this->NUM_SAMPLES_PER_SEGMENT > this->NUM_SAMPLES_PER_SEGMENT * this->SEGMENTS_PER_FRAME) {
      LOG_ERROR("Window exceeds audio_data range.");
      return AUDIO2IMAGE_RET_T::UNFILLED_MATRIX;
    }

    // Extract the current sliding window of audio data (16 samples)
    for (int i = 0; i < this->NUM_SAMPLES_PER_SEGMENT; ++i) {
      in[i][0] = audio_data[window_start + i];  // Real part
      in[i][1] = 0;                             // Imaginary part
    }

    // Execute the FFT for this window
    fftw_execute(p);

    this->print_complex_fft(out, this->NUM_SAMPLES_PER_SEGMENT);

    // Compute the average frequency and amplitude for the current window
    auto [average_frequency, average_amplitude] =
        this->compute_average_frequency_and_amplitude(out,
                                                      this->NUM_SAMPLES_PER_SEGMENT);

    // Normalize the average frequency and amplitude to pixel values
    auto [norm_average_frequency, norm_average_amplitude] =
        this->normalize_to_pixel_values(average_frequency, average_amplitude);

    // Compute pixel coordinates based on pixel_count
    int y_pixel = pixel_count / this->IMAGE_SIZE_X_PIXELS;
    int x_pixel = pixel_count % this->IMAGE_SIZE_X_PIXELS;

    // Check if the pixel is within bounds of the image
    if (y_pixel >= this->IMAGE_SIZE_Y_PIXELS ||
        x_pixel >= this->IMAGE_SIZE_X_PIXELS) {
      LOG_ERROR("UNFILLED MATRIX");
      return AUDIO2IMAGE_RET_T::UNFILLED_MATRIX;
    }

    // Set the pixel values in the result image
    cv::Vec3b& pixel = result_image.at<cv::Vec3b>(y_pixel, x_pixel);
    pixel[0] = norm_average_frequency;  // Blue channel (normalized frequency)
    pixel[1] = 0;                       // Green channel (unused)
    pixel[2] = norm_average_amplitude;  // Red channel (normalized amplitude)

    // Increment pixel_count for the next iteration
    pixel_count++;

    // Log the results for debugging
    LOG_INFO(std::format(
        "Norm average frequency: {}, and norm average amplitude: {}",
        norm_average_frequency, norm_average_amplitude));
    LOG_INFO(std::format("Cell X:{} Y:{} updated.", x_pixel, y_pixel));
  }

  // Clean up FFT resources
  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);

  return AUDIO2IMAGE_RET_T::GOOD_FRAME_INSERTION_TO_IMAGE;
}

double Audio2Image::get_audio_duration(const std::string filename) {
  // Check if the file has a valid audio extension
  if (!(std::regex_match(filename, audio_file_regex.WAV) ||
        std::regex_match(filename, audio_file_regex.AIFF) ||
        std::regex_match(filename, audio_file_regex.FLAC))) {
    LOG_WARNING("Audio file did not match a valid extension.");
    return -1.0;  // Invalid file format
  }

  // Initialize FFmpeg
  AVFormatContext* format_ctx = nullptr;
  if (avformat_open_input(&format_ctx, filename.c_str(), nullptr, nullptr) !=
      0) {
    LOG_ERROR("AVformat could not open file.");
    return -1.0;
  }

  // Retrieve stream info
  if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
    LOG_ERROR("Error: Could not retrieve stream info");
    avformat_close_input(&format_ctx);
    return -1.0;
  }

  // Get duration in seconds
  double duration = static_cast<double>(format_ctx->duration) / AV_TIME_BASE;

  // Clean up
  avformat_close_input(&format_ctx);

  return duration;
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
    LOG_ERROR("Invalid file duration. Duration must be >= 10.0 seconds.");
    return false;
  }

  std::ostringstream command;
  command << "ffmpeg -i \"" << filename << "\" -t " << new_duration
          << " -c copy \"" << new_filename << "\" -y 2>&1";

  FILE* pipe = popen(command.str().c_str(), "r");
  if (!pipe) {
    LOG_ERROR("Failed to run FFmpeg command.");
    return false;
  }

  char buffer[128];
  std::string result = "";
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  int exit_code = pclose(pipe);

  if (exit_code == 0) {
    LOG_INFO(std::format("Audio clipped to {}", new_duration));
    return true;
  } else {
    LOG_ERROR("Error occurred during audio clipping.");
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

/*==========================================
=                  PUBLIC                  =
==========================================*/

std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> Audio2Image::audio_file_to_image(
    std::string filename) {
  cv::Mat result_image(this->IMAGE_SIZE_X_PIXELS, this->IMAGE_SIZE_Y_PIXELS,
                       CV_8UC3, cv::Scalar(0, 0, 0));
  int pixel_count = 0;

  // Initialize FFmpeg
  avformat_network_init();

  AVFormatContext* format_ctx = nullptr;
  if (avformat_open_input(&format_ctx, filename.c_str(), nullptr, nullptr) !=
      0) {
    LOG_ERROR("FFMPEG ERROR OPENING AUDIO FILE");
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_OPENING_AUDIO_FILE, cv::Mat()};
  }

  if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
    LOG_ERROR("FFMPEG ERROR FINDING AUDIO STREAM INFO");
    return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_FINDING_AUDIO_STREAM_INFO,
            cv::Mat()};
  }

  AVCodec* codec = nullptr;
  AVCodecContext* codec_ctx = nullptr;
  AVStream* audio_stream = nullptr;
  for (int i = 0; i < format_ctx->nb_streams; ++i) {
    audio_stream = format_ctx->streams[i];
    codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (codec) {
      codec_ctx = avcodec_alloc_context3(codec);
      if (avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar) <
          0) {
        LOG_ERROR("FFMPEG ERROR COPY CODEC PARAM TO CONTEXT");
        return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_COPY_CODEC_PARAM_TO_CONTEXT,
                cv::Mat()};
      }
      if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        LOG_ERROR("FFMPEG CANNOT OPEN CODED");
        return {AUDIO2IMAGE_RET_T::FFMPEG_CANNOT_OPEN_CODEC, cv::Mat()};
      }
      break;
    }
  }

  if (!audio_stream) {
    std::cerr << "No audio stream found." << std::endl;
    LOG_ERROR("FFMPEG AUDIO STREAM NOT FOUND");
    return {AUDIO2IMAGE_RET_T::FFMPEG_AUDIO_STREAM_NOT_FOUND, cv::Mat()};
  }

  SwrContext* swr_ctx = swr_alloc();
  swr_alloc_set_opts(swr_ctx, AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_FLT,
                     codec_ctx->sample_rate, codec_ctx->channel_layout,
                     codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, nullptr);
  swr_init(swr_ctx);

  // Allocate frame for reading data
  AVFrame* frame = av_frame_alloc();
  AVPacket packet;
  std::vector<float> audio_data;

  // Read frames and process audio
  while (av_read_frame(format_ctx, &packet) >= 0) {
    if (packet.stream_index == audio_stream->index) {
      int ret = avcodec_send_packet(codec_ctx, &packet);
      if (ret < 0) {
        LOG_ERROR("FFMPEG ERROR SENDING PACKET TO CODEC");
        return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_SENDING_PACKET_TO_CODEC,
                cv::Mat()};
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }
        if (ret < 0) {
          LOG_ERROR("FFMPEG ERROR RECEIVING FRAME FROM CODEC");
          return {AUDIO2IMAGE_RET_T::FFMPEG_ERROR_RECEIVING_FRAME_FROM_CODEC,
                  cv::Mat()};
        }

        // Convert audio samples to float
        int num_samples = frame->nb_samples;
        audio_data.resize(num_samples);
        for (int i = 0; i < num_samples; ++i) {
          audio_data[i] =
              frame->data[0][i] / 32768.0f;  // Assuming 16-bit PCM data
        }

        this->insert_codec_frame_to_image(pixel_count, audio_data,
                                          result_image);
      }
    }

    av_packet_unref(&packet);
  }

  // Clean up
  av_frame_free(&frame);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&format_ctx);
  swr_free(&swr_ctx);

  // Fill empty matrix cells if necessary
  if (pixel_count <= this->IMAGE_SIZE_X_PIXELS * IMAGE_SIZE_Y_PIXELS) {
    LOG_INFO("Filling matrix with valid values");

    while (pixel_count <= this->IMAGE_SIZE_X_PIXELS * IMAGE_SIZE_Y_PIXELS) {
      int y_pixel = pixel_count / this->IMAGE_SIZE_Y_PIXELS;
      int x_pixel = pixel_count % this->IMAGE_SIZE_X_PIXELS;

      cv::Vec3b& pixel = result_image.at<cv::Vec3b>(y_pixel, x_pixel);
      pixel[0] = 0;  // Blue channel
      pixel[1] = 0;  // Green channel
      pixel[2] = 0;  // Red channel
      pixel_count++;
    }
  }

  // Return result depending on whether the matrix was filled
  return (pixel_count == this->IMAGE_SIZE_X_PIXELS * this->IMAGE_SIZE_Y_PIXELS)
             ? std::make_tuple(AUDIO2IMAGE_RET_T::GOOD_CONVERSION, result_image)
             : std::make_tuple(AUDIO2IMAGE_RET_T::UNFILLED_MATRIX,
                               result_image);
}

std::tuple<AUDIO2IMAGE_RET_T, cv::Mat> Audio2Image::audio2image(
    std::string filename) {
  // Verify audio type is valid
  if (!(std::regex_match(filename, audio_file_regex.WAV) ||
        std::regex_match(filename, audio_file_regex.AIFF) ||
        std::regex_match(filename, audio_file_regex.FLAC))) {
    LOG_ERROR("INVALID AUDIO FILE TYPE");
    return {AUDIO2IMAGE_RET_T::INVALID_AUDIO_FILE_TYPE,
            cv::Mat::zeros(0, 0, CV_8UC3)};
  }

  if (this->get_audio_duration(filename) < 10.0) {
    LOG_ERROR("AUDIO FILE DURATION TOO SHORT");
    return {AUDIO2IMAGE_RET_T::AUDIO_FILE_DURATION_TOO_SHORT,
            cv::Mat::zeros(0, 0, CV_8UC3)};

  } else if (this->get_audio_duration(filename) > 10.0) {
    std::string new_filename = this->generateClippedFilename(
        filename, this->AUDIO_DURATION_SEC);  // filename-10sec.format
    if (!this->clip_audio_file(filename, new_filename,
                               this->AUDIO_DURATION_SEC)) {
      LOG_ERROR("CANNOT CLIP AUDIO FILE");
      return {AUDIO2IMAGE_RET_T::CANNOT_CLIP_AUDIO_FILE,
              cv::Mat::zeros(0, 0, CV_8UC3)};
    }
    filename = new_filename;
  }

  // TODO add call to audio_file_to_image()

  LOG_INFO("GOOD AUDIO TO IMAGE CONVERSION");
  return {AUDIO2IMAGE_RET_T::GOOD_AUDIO2IMAGE, cv::Mat::zeros(0, 0, CV_8UC3)};
}
