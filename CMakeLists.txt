cmake_minimum_required(VERSION 3.25)

project(
  MeetFlow
  VERSION 0.1.0
  DESCRIPTION "Local-first meeting transcription and minutes tools"
  LANGUAGES CXX
)

option(MEETFLOW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MEETFLOW_ENABLE_SHERPA_ONNX "Enable the native Sherpa-ONNX inference runtime" OFF)
option(MEETFLOW_DOWNLOAD_MODELS "Download missing Sherpa-ONNX model files during configure" OFF)
set(MEETFLOW_SHERPA_ONNX_ROOT "" CACHE PATH
    "Sherpa-ONNX installation prefix containing include/ and lib/")
set(MEETFLOW_MODELS_DIR "${CMAKE_BINARY_DIR}/models" CACHE PATH
    "Local directory for MeetFlow model files")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MEETFLOW_DOWNLOAD_MODELS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MeetFlowModels.cmake")
  meetflow_prepare_models("${MEETFLOW_MODELS_DIR}")
endif()

add_library(meetflow_build_settings INTERFACE)
target_compile_features(meetflow_build_settings INTERFACE cxx_std_20)

if(MEETFLOW_ENABLE_SHERPA_ONNX)
  find_path(MEETFLOW_SHERPA_ONNX_INCLUDE_DIR
    NAMES cxx-api.h
    HINTS
      "${MEETFLOW_SHERPA_ONNX_ROOT}/include"
      "${MEETFLOW_SHERPA_ONNX_ROOT}/sherpa-onnx/c-api"
      "${MEETFLOW_SHERPA_ONNX_ROOT}/c-api"
    REQUIRED
  )
  find_library(MEETFLOW_SHERPA_ONNX_CXX_LIBRARY
    NAMES sherpa-onnx-cxx-api
    HINTS
      "${MEETFLOW_SHERPA_ONNX_ROOT}/lib"
      "${MEETFLOW_SHERPA_ONNX_ROOT}/build/lib"
    REQUIRED
  )
  find_library(MEETFLOW_SHERPA_ONNX_C_LIBRARY
    NAMES sherpa-onnx-c-api
    HINTS
      "${MEETFLOW_SHERPA_ONNX_ROOT}/lib"
      "${MEETFLOW_SHERPA_ONNX_ROOT}/build/lib"
    REQUIRED
  )
  find_library(MEETFLOW_ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    HINTS
      "${MEETFLOW_SHERPA_ONNX_ROOT}/lib"
      "${MEETFLOW_SHERPA_ONNX_ROOT}/build/_deps/onnxruntime-src/lib"
    REQUIRED
  )

  add_library(meetflow_sherpa_onnx INTERFACE)
  target_include_directories(meetflow_sherpa_onnx INTERFACE
    "${MEETFLOW_SHERPA_ONNX_INCLUDE_DIR}"
    "${MEETFLOW_SHERPA_ONNX_ROOT}"
    "${MEETFLOW_SHERPA_ONNX_ROOT}/include"
  )
  target_link_libraries(meetflow_sherpa_onnx INTERFACE
    "${MEETFLOW_SHERPA_ONNX_CXX_LIBRARY}"
    "${MEETFLOW_SHERPA_ONNX_C_LIBRARY}"
    "${MEETFLOW_ONNXRUNTIME_LIBRARY}"
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
  set_property(TARGET meetflow-prepare APPEND PROPERTY BUILD_RPATH
    "${MEETFLOW_SHERPA_ONNX_ROOT}/lib"
    "${MEETFLOW_SHERPA_ONNX_ROOT}/build/lib"
    "${MEETFLOW_SHERPA_ONNX_ROOT}/build/_deps/onnxruntime-src/lib"
  )
endif()
if(APPLE)
  target_link_libraries(meetflow-prepare PRIVATE
    "-framework AudioToolbox"
    "-framework CoreFoundation"
  )
endif()
add_meetflow_cli(meetflow-draft)
add_meetflow_cli(meetflow-finalize)
