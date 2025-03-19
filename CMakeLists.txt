cmake_minimum_required(VERSION 3.10)
project(Audiovisual)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    include
    /usr/include/opencv4
    /usr/local/include/opencv4
    ${CMAKE_SOURCE_DIR}/include
    /usr/include
)

# Find all source files recursively in src/
file(GLOB_RECURSE SRCS src/*.cpp)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)

# Manually specify FFTW3 include directory and libraries
set(FFTW3_INCLUDE_DIR /usr/include)  # Manually set the include directory
set(FFTW3_LIBRARIES fftw3)  # Manually link to the fftw3 library

# Add the necessary FFTW3 include directories
include_directories(${FFTW3_INCLUDE_DIR})

# Add the necessary FFTW3 libraries
set(FFMPEG_EXECUTABLE "/usr/bin/ffmpeg")

# Find libraries using pkg-config
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

# Linker libraries
set(OPENCV_LIBS opencv_core opencv_imgcodecs opencv_imgproc opencv_highgui opencv_videoio)
set(FFTW_LIBS ${FFTW3_LIBRARIES})  # Link to fftw3
set(FFMPEG_LIBS ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWSCALE_LIBRARIES})
set(OTHER_LIBS wiringPi)

# Include directories for FFmpeg and FFTW
include_directories(${AVCODEC_INCLUDE_DIRS} ${AVFORMAT_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Main executable
add_executable(main ${SRCS})
target_link_libraries(main PRIVATE ${OPENCV_LIBS} ${FFTW_LIBS} ${FFMPEG_LIBS} ${OTHER_LIBS})

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "bin/main")
