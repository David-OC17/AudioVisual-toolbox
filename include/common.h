#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdlib>

#include <fftw3.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

#define CD_AUDIO_FILE_FREQUENCY_HZ 44100
#define MAX_FREQUENCY_IN_AUDIO_SIGNAL_HZ 50000.0F
#define MAX_PIXEL_VALUE 255.0F

#define SQUARE_IMG_SIZE_X 525
#define SQUARE_IMG_SIZE_Y 525

struct CDAudioFormatsRegex {
  const std::regex WAV{R"((?:[\w\-\/\.]+\/)*[\w\-\.]+\.WAV$)",
                       std::regex::icase};
  const std::regex AIFF{R"((?:[\w\-\/\.]+\/)*[\w\-\.]+\.AIFF$)",
                        std::regex::icase};
  const std::regex FLAC{R"((?:[\w\-\/\.]+\/)*[\w\-\.]+\.FLAC$)",
                        std::regex::icase};
};

static CDAudioFormatsRegex audio_file_regex;

#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define GREEN "\033[32m"
#define RESET "\033[0m"

#define LOG_ERROR(msg) log_message("ERROR", RED, __FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_WARNING(msg) log_message("WARNING", YELLOW, __FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_INFO(msg) log_message("INFO", BLUE, __FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_SUCCESS(msg) log_message("INFO", GREEN, __FILE__, __LINE__, __FUNCTION__, msg)

/* Function to log messages with additional context */
inline void log_message(const std::string& level, const std::string& color,
                        const std::string& file, int line, const std::string& func,
                        const std::string& msg) {
  std::time_t now = std::time(nullptr);
  std::tm* tm_info = std::localtime(&now);
  char timestamp[20];
  std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

  // Print the log message with context and color
  std::cout << "[" << timestamp << "] ["
            << color << level << RESET << "] "
            << "[" << file << ":" << line << "] "
            << "[" << func << "] " << msg << std::endl;
}