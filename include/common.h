#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>
}

#include <fftw3.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <regex>

#define CD_AUDIO_FILE_FREQUENCY_HZ 44100

struct CDAudioFormatsRegex {
    static const std::regex WAV;
    static const std::regex AIFF;
    static const std::regex FLAC;
};

const std::regex CDAudioFormatsRegex::WAV(R"([a-zA-Z0-9_\- ]+\.WAV$)", std::regex::icase);
const std::regex CDAudioFormatsRegex::AIFF(R"([a-zA-Z0-9_\- ]+\.AIFF$)", std::regex::icase);
const std::regex CDAudioFormatsRegex::FLAC(R"([a-zA-Z0-9_\- ]+\.FLAC$)", std::regex::icase);
