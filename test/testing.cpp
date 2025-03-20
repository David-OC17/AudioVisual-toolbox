#include <iostream>

#include "common.h"

void TEST_libraries();
void TEST_audio2image();

/* Simple test for FFTW, OpenCV and FFmpeg libraries. */
void TEST_libraries() {
    // Test FFTW
    std::cout << "Testing FFTW..." << std::endl;

    fftw_complex in[8], out[8];
    for (int i = 0; i < 8; ++i) {
        in[i][0] = i + 1;
        in[i][1] = 0;
    }
    fftw_plan p = fftw_plan_dft_1d(8, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);
    std::cout << "FFTW transform result (output):" << std::endl;
    for (int i = 0; i < 8; ++i) {
        std::cout << "out[" << i << "] = (" << out[i][0] << ", " << out[i][1] << ")\n";
    }
    fftw_destroy_plan(p);

    // Test OpenCV
    std::cout << "\nTesting OpenCV..." << std::endl;
    cv::Mat image = cv::imread("../data/test_image-cat.jpg");
    if (image.empty()) {
        std::cout << "Error: Could not load image!" << std::endl;
    } else {
        cv::imshow("Test Image", image);
        cv::waitKey(0);
    }

    // Test FFmpeg
    std::cout << "\nTesting FFmpeg..." << std::endl;
    av_register_all();
    const char* filename = "../data/test_video-cat.mp4";  // Replace with actual path to a video file
    AVFormatContext* pFormatCtx = nullptr;
    if (avformat_open_input(&pFormatCtx, filename, nullptr, nullptr) != 0) {
        std::cout << "Error: Could not open video file!" << std::endl;
    } else {
        std::cout << "FFmpeg successfully opened the video file!" << std::endl;
        avformat_close_input(&pFormatCtx);  // Close the file after testing
    }
}

void TEST_audio2image() {

}