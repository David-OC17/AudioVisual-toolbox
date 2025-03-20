#include <iostream>

#include "common.h"

void TEST_libraries();
void TEST_audio2image();

/* Simple test for FFTW, OpenCV and FFmpeg libraries. */
void TEST_libraries() {
    // Test FFTW
    std::cout << "Testing FFTW..." << std::endl;

    // Create an array of 8 complex numbers (real and imaginary parts)
    fftw_complex in[8], out[8];

    // Initialize input data (just example values)
    for (int i = 0; i < 8; ++i) {
        in[i][0] = i + 1;  // Real part
        in[i][1] = 0;      // Imaginary part
    }

    // Perform FFT (in-place transform)
    fftw_plan p = fftw_plan_dft_1d(8, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    std::cout << "FFTW transform result (output):" << std::endl;
    for (int i = 0; i < 8; ++i) {
        std::cout << "out[" << i << "] = (" << out[i][0] << ", " << out[i][1] << ")\n";
    }

    fftw_destroy_plan(p);

    // Test OpenCV
    std::cout << "\nTesting OpenCV..." << std::endl;

    // Load an image (replace with your image file path)
    cv::Mat image = cv::imread("../test/test_image.jpg");

    if (image.empty()) {
        std::cout << "Error: Could not load image!" << std::endl;
    } else {
        cv::imshow("Test Image", image);  // Show the image
        cv::waitKey(0);  // Wait until a key is pressed
    }

    // Test FFmpeg
    std::cout << "\nTesting FFmpeg..." << std::endl;

    // Initialize FFmpeg
    av_register_all();

    // Open a test video file (replace with your video file path)
    const char* filename = "../test/test_video.mp4";  // Replace with actual path to a video file
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