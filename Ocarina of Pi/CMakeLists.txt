cmake_minimum_required(VERSION 3.16)
project(OcarinaOfPi CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SDL2_PATH "${CMAKE_SOURCE_DIR}/../SDL2-RPI" CACHE PATH "Path to SDL2 RPI installation")

if(NOT CMAKE_CROSSCOMPILING)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR armv7l)
endif()

set(TOOLCHAIN_PREFIX arm-linux-gnueabihf-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX} /usr/arm-linux-gnueabihf /opt/vc)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3 -ftree-vectorize -fomit-frame-pointer -flto")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3 -ftree-vectorize -fomit-frame-pointer -flto -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-O3 -Wl,--as-needed -Wl,--no-warn-rwx-segments")

find_library(GLES2_LIB GLESv2 HINTS /opt/vc/lib /usr/lib/arm-linux-gnueabihf)
find_library(EGL_LIB EGL HINTS /opt/vc/lib /usr/lib/arm-linux-gnueabihf)
find_library(BCM_HOST_LIB bcm_host HINTS /opt/vc/lib /usr/lib/arm-linux-gnueabihf)
find_library(SDL2_LIB SDL2 HINTS ${SDL2_PATH}/lib /usr/lib/arm-linux-gnueabihf)
find_library(SDL2GFX_LIB SDL2_gfx HINTS ${SDL2_PATH}/lib /usr/lib/arm-linux-gnueabihf)
find_library(PTHREAD_LIB pthread HINTS /usr/lib/arm-linux-gnueabihf)
find_library(DL_LIB dl HINTS /usr/lib/arm-linux-gnueabihf)
find_library(RT_LIB rt HINTS /usr/lib/arm-linux-gnueabihf)
find_library(ASOUND_LIB asound HINTS /usr/lib/arm-linux-gnueabihf)
find_library(M_LIB m HINTS /usr/lib/arm-linux-gnueabihf)

if(NOT GLES2_LIB OR NOT SDL2_LIB)
    message(FATAL_ERROR "Required libraries not found: GLESv2=${GLES2_LIB} SDL2=${SDL2_LIB}")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${SDL2_PATH}/include
    /opt/vc/include
    /usr/include/freetype2
    /usr/include/libdrm
    /usr/include/drm
)

set(SOURCES
    src/main.cpp
    src/video_rpi.cpp
    src/audio_alsa.cpp
    src/threading.cpp
    src/mem_manager.cpp
    src/game_input.cpp
)

set(HEADERS
    include/video_rpi.h
    include/audio_alsa.h
    include/threading.h
    include/mem_manager.h
    include/game_input.h
    include/main.h
)

add_executable(ocarina_of_pi ${SOURCES} ${HEADERS})

target_link_libraries(ocarina_of_pi
    ${GLES2_LIB}
    ${EGL_LIB}
    ${BCM_HOST_LIB}
    ${SDL2_LIB}
    ${SDL2GFX_LIB}
    ${PTHREAD_LIB}
    ${DL_LIB}
    ${RT_LIB}
    ${ASOUND_LIB}
    ${M_LIB}
    vcos
    vchiq_arm
)

install(TARGETS ocarina_of_pi RUNTIME DESTINATION bin)
install(FILES config/config.txt DESTINATION .)