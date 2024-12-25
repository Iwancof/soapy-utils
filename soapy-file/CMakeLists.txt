########################################################################
# Project setup -- only needed if device support is a stand-alone build
# We recommend that the support module be built in-tree with the driver.
########################################################################
cmake_minimum_required(VERSION 2.6)
project(SoapySDRMyDevice CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

########################################################################
# Header and library resources needed to communicate with the device.
# These may be found within the build tree or in an external project.
########################################################################
set(FILE_DEVICE_INCLUDE_DIRS "")
set(FILE_DEVICE_LIBRARIES "")

########################################################################
# build the module
########################################################################
find_package(SoapySDR CONFIG)

if (NOT SoapySDR_FOUND)
    message(WARNING "SoapySDR development files not found - skipping support")
    return()
endif ()

include_directories(${FILE_DEVICE_INCLUDE_DIRS})
SOAPY_SDR_MODULE_UTIL(
    TARGET FileDevice
    SOURCES FileDeviceSupport.cpp
    LIBRARIES ${FILE_DEVICE_LIBRARIES}
)
