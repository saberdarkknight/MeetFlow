cmake_minimum_required(VERSION 3.25)

project(
  MeetFlow
  VERSION 0.1.0
  DESCRIPTION "Local-first meeting transcription and minutes tools"
  LANGUAGES CXX
)

option(MEETFLOW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MEETFLOW_ENABLE_SHERPA_ONNX "Enable the native Sherpa-ONNX inference runtime" OFF)
set(MEETFLOW_SHERPA_ONNX_ROOT "" CACHE PATH
    "Sherpa-ONNX installation prefix containing include/ and lib/")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(meetflow_build_settings INTERFACE)
target_compile_features(meetflow_build_settings INTERFACE cxx_std_20)

if(MEETFLOW_ENABLE_SHERPA_ONNX)
  find_path(MEETFLOW_SHERPA_ONNX_INCLUDE_DIR
    NAMES cxx-api.h
    HINTS "${MEETFLOW_SHERPA_ONNX_ROOT}/include"
    REQUIRED
  )
  find_library(MEETFLOW_SHERPA_ONNX_LIBRARY
    NAMES sherpa-onnx-c-api
    HINTS "${MEETFLOW_SHERPA_ONNX_ROOT}/lib"
    REQUIRED
  )

  add_library(meetflow_sherpa_onnx INTERFACE)
  target_include_directories(meetflow_sherpa_onnx INTERFACE
    "${MEETFLOW_SHERPA_ONNX_INCLUDE_DIR}"
  )
  target_link_libraries(meetflow_sherpa_onnx INTERFACE
    "${MEETFLOW_SHERPA_ONNX_LIBRARY}"
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(meetflow_build_settings INTERFACE -Wall -Wextra -Wpedantic)
  if(MEETFLOW_WARNINGS_AS_ERRORS)
    target_compile_options(meetflow_build_settings INTERFACE -Werror)
  endif()
elseif(MSVC)
  target_compile_options(meetflow_build_settings INTERFACE /W4)
  if(MEETFLOW_WARNINGS_AS_ERRORS)
    target_compile_options(meetflow_build_settings INTERFACE /WX)
  endif()
endif()

function(add_meetflow_cli target)
  add_executable(${target} "${CMAKE_CURRENT_SOURCE_DIR}/src/${target}/main.cpp")
  target_link_libraries(${target} PRIVATE meetflow_build_settings)
endfunction()

add_meetflow_cli(meetflow-prepare)
if(MEETFLOW_ENABLE_SHERPA_ONNX)
  target_link_libraries(meetflow-prepare PRIVATE meetflow_sherpa_onnx)
  target_compile_definitions(meetflow-prepare PRIVATE MEETFLOW_ENABLE_SHERPA_ONNX=1)
endif()
if(APPLE)
  target_link_libraries(meetflow-prepare PRIVATE
    "-framework AudioToolbox"
    "-framework CoreFoundation"
  )
endif()
add_meetflow_cli(meetflow-draft)
add_meetflow_cli(meetflow-finalize)
