# Toolchain file for Raspberry Pi 2B cross-compilation
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-armpi.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

set(TOOLCHAIN_PREFIX arm-linux-gnueabihf-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf /opt/vc)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3 -ftree-vectorize -fomit-frame-pointer -flto")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3 -ftree-vectorize -fomit-frame-pointer -flto -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-O3 -Wl,--as-needed")