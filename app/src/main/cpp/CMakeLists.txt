cmake_minimum_required(VERSION 3.4.1)
# set targetPlatform, will be passed in from gradle when this sample is completed
# openGL Supportability
# platform         status
#   (0 12)           ES2/ES3 not supported
#   [12, 18)         ES2 only; for ES3, app do dynamic load/detection
#                    this applies to the situations that:
#                        - minimum API is set to less than 18. In this case
#                          there is no ES3 header/lib support inside NDK
#                        - the built APK might be running on newer API phones
#                    with dynamic loading of ES3, the same APK would still be able
#                    to use ES3. Otherwise, app would stuck with ES2 even phone is
#                    is newer than the minimum API level (for example, Android-27 etc).
#
#   [18, 24)         ES2 & ES3
#                    If app is built to only support API-18 or later,
#                    set minimum api level to 18 is good enough, NDK supprts ES3
#                    with the right header and lib files. No need to use ES3 dynamic
#                    detection.
#   [24, infinite)   ES2 & ES3 & Vulkan

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
set(OPENGL_LIB GLESv3)

add_subdirectory(subprojects/hdr2sdr/third-party/project/libjpeg-turbo)

add_library(gles3jni SHARED
	gles3jni.cpp
	opengl-helper.cpp
	perf-monitor.cpp
	render.cpp
	subprojects/hdr2sdr/hdr_decoder.cpp
	subprojects/hdr2sdr/image.cpp
	subprojects/hdr2sdr/image_decoder.cpp
	subprojects/hdr2sdr/image_encoder.cpp
	subprojects/hdr2sdr/image_merge.cpp
	subprojects/hdr2sdr/ldr_decoder.cpp
	subprojects/hdr2sdr/openexr_decoder.cpp
	subprojects/hdr2sdr/params.cpp
	subprojects/hdr2sdr/pfm_decoder.cpp
	subprojects/hdr2sdr/tonemapper.cpp
	subprojects/hdr2sdr/stb_image.c)

target_include_directories(gles3jni PRIVATE
	android
	subprojects/hdr2sdr
	subprojects/hdr2sdr/include
	subprojects/hdr2sdr/third-party/project/libjpeg-turbo
	${CMAKE_CURRENT_BINARY_DIR}/subprojects/hdr2sdr/third-party/project/libjpeg-turbo) #for jconfig.h

# Include libraries needed for gles3jni lib
target_link_libraries(gles3jni
            jpeg
            ${OPENGL_LIB}
            android
            EGL
            log
            m)
