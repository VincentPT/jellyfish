cmake_minimum_required(VERSION 3.2.0)

# set include directories
SET (I_CINDER_IMGUI_BLOCK_DIR "")
SET (I_CINDER_IMGUI_INCLUDE_DIR "")
SET (I_CINDER_IMGUI_LIB_DIR "")

# if CINDER_INCLUDE_DIR is defined, set include dir
if (DEFINED CINDER_INCLUDE_DIR)
    SET(I_CINDER_IMGUI_BLOCK_DIR ${CINDER_INCLUDE_DIR}/../blocks/Cinder-ImGui)
    SET(I_CINDER_IMGUI_INCLUDE_DIR ${I_CINDER_IMGUI_BLOCK_DIR}/include)
    SET(I_CINDER_IMGUI_LIB_DIR ${I_CINDER_IMGUI_BLOCK_DIR}/lib/msw/x64)
endif()

# apply opencv inlcude dir to cmake
if (NOT ${I_CINDER_IMGUI_INCLUDE_DIR} EQUAL "")
    target_include_directories(${PROJECT_NAME} PRIVATE ${I_CINDER_IMGUI_INCLUDE_DIR})
    target_include_directories(${PROJECT_NAME} PRIVATE ${I_CINDER_IMGUI_BLOCK_DIR}/lib/imgui)
endif()

find_library (CINDER_IMGUI_D cinder_imgui PATHS ${I_CINDER_IMGUI_LIB_DIR} PATH_SUFFIXES Debug_Shared)
find_library (CINDER_IMGUI_R cinder_imgui PATHS ${I_CINDER_IMGUI_LIB_DIR} PATH_SUFFIXES Release_Shared)

# if(WIN32)
    # string(REGEX REPLACE "/./lib" ".dll" CINDER_IMGUI_BIN_D ${CINDER_IMGUI_D})
    # string(REGEX REPLACE "/./lib" ".dll" CINDER_IMGUI_BIN_R ${CINDER_IMGUI_R})
# endif()

if(WIN32)
get_filename_component(CINDER_IMGUI_BIN_D ${CINDER_IMGUI_D} DIRECTORY)
get_filename_component(CINDER_IMGUI_BIN_R ${CINDER_IMGUI_R} DIRECTORY)
set (CINDER_IMGUI_BIN_D ${CINDER_IMGUI_BIN_D}/cinder_imgui.dll)
set (CINDER_IMGUI_BIN_R ${CINDER_IMGUI_BIN_R}/cinder_imgui.dll)
endif()

target_link_libraries(${PROJECT_NAME} debug "${CINDER_IMGUI_D}" optimized "${CINDER_IMGUI_R}")