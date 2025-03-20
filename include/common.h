#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

#include <fftw3.h>

#include <ctime>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <regex>
#include <sstream>

#define CD_AUDIO_FILE_FREQUENCY_HZ 44100

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
#define RESET "\033[0m"

#define LOG_ERROR(msg) log_message("ERROR", RED, __FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_WARNING(msg) log_message("WARNING", YELLOW, __FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_INFO(msg) log_message("INFO", BLUE, __FILE__, __LINE__, __FUNCTION__, msg)

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