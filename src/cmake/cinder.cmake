cmake_minimum_required(VERSION 3.2.0)

# set include directories
SET (I_CINDER_INCLUDE_DIR "")
SET (I_CINDER_LIB_DIR "")

# if CINDER_INCLUDE_DIR is defined, set include dir
if (DEFINED CINDER_INCLUDE_DIR)
    SET(I_CINDER_INCLUDE_DIR ${CINDER_INCLUDE_DIR})
endif()

# if CINDER_LIB_DIR is defined, set library dir
if (DEFINED CINDER_LIB_DIR)
    SET(I_CINDER_LIB_DIR ${CINDER_LIB_DIR})
endif()

# apply opencv inlcude dir to cmake
if (NOT ${I_CINDER_INCLUDE_DIR} EQUAL "")
    target_include_directories(${PROJECT_NAME} PRIVATE ${I_CINDER_INCLUDE_DIR})
endif()

find_library (CINDER_D cinder PATHS ${I_CINDER_LIB_DIR} PATH_SUFFIXES Debug_Shared)
find_library (CINDER_R cinder PATHS ${I_CINDER_LIB_DIR} PATH_SUFFIXES Release_Shared)

# if(WIN32)
    # string(REGEX REPLACE "\./glib" ".dll" CINDER_BIN_D ${CINDER_D})
    # string(REGEX REPLACE "\./glib" ".dll" CINDER_BIN_R ${CINDER_R})
# endif()
if(WIN32)
get_filename_component(CINDER_BIN_D ${CINDER_D} DIRECTORY)
get_filename_component(CINDER_BIN_R ${CINDER_R} DIRECTORY)
set (CINDER_BIN_D ${CINDER_BIN_D}/cinder.dll)
set (CINDER_BIN_R ${CINDER_BIN_R}/cinder.dll)
endif()

target_link_libraries(${PROJECT_NAME} debug "${CINDER_D}" optimized "${CINDER_R}")