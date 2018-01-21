cmake_minimum_required(VERSION 3.2.0)

SET (TARGET_INTERFACE cpprestsdk)

# set include directories
SET (I_CPPRESTSDK_INCLUDE_DIR "")
SET (I_CPPRESTSDK_LIB_DIR "")

# if CPPRESTSDK_INCLUDE_DIR is defined, set include dir
if (DEFINED CPPRESTSDK_INCLUDE_DIR)
    SET(I_CPPRESTSDK_INCLUDE_DIR ${CPPRESTSDK_INCLUDE_DIR})
endif()

# if CPPRESTSDK_LIB_DIR is defined, set library dir
if (DEFINED CPPRESTSDK_LIB_DIR)
    SET(I_CPPRESTSDK_LIB_DIR ${CPPRESTSDK_LIB_DIR})
endif()

# apply the target to cmake
add_library(${TARGET_INTERFACE} INTERFACE)

# apply include dir to cmake
if (NOT ${I_CPPRESTSDK_INCLUDE_DIR} EQUAL "")
    target_include_directories(${TARGET_INTERFACE} INTERFACE ${I_CPPRESTSDK_INCLUDE_DIR})
endif()

find_library (CPPRESTSDK_D cpprest140d_2_9 PATHS ${I_CPPRESTSDK_LIB_DIR} PATH_SUFFIXES debug)
find_library (CPPRESTSDK_R cpprest140_2_9 PATHS ${I_CPPRESTSDK_LIB_DIR} PATH_SUFFIXES release)

target_link_libraries(${TARGET_INTERFACE} INTERFACE debug "${CPPRESTSDK_D}" INTERFACE optimized "${CPPRESTSDK_R}")