include_guard(GLOBAL)

set(MEETFLOW_WHISPER_MODEL_URL
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2"
    CACHE STRING "URL for the multilingual Whisper model archive")
set(MEETFLOW_DIARIZATION_SEGMENTATION_URL
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-segmentation-models/sherpa-onnx-pyannote-segmentation-3-0.tar.bz2"
    CACHE STRING "URL for the speaker segmentation model archive")
set(MEETFLOW_DIARIZATION_EMBEDDING_URL
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models/3dspeaker_speech_eres2net_base_sv_zh-cn_3dspeaker_16k.onnx"
    CACHE STRING "URL for the speaker embedding model")

function(_meetflow_download url destination)
  if(EXISTS "${destination}")
    message(STATUS "MeetFlow model exists; skipping download: ${destination}")
    return()
  endif()

  get_filename_component(destination_parent "${destination}" DIRECTORY)
  file(MAKE_DIRECTORY "${destination_parent}")
  message(STATUS "Downloading MeetFlow model: ${url}")
  file(DOWNLOAD "${url}" "${destination}.part"
       TLS_VERIFY ON
       SHOW_PROGRESS
       STATUS download_status)
  list(GET download_status 0 download_code)
  list(GET download_status 1 download_message)
  if(NOT download_code EQUAL 0)
    file(REMOVE "${destination}.part")
    message(FATAL_ERROR "Could not download ${url}: ${download_message}")
  endif()
  file(RENAME "${destination}.part" "${destination}")
endfunction()

function(_meetflow_extract_archive archive extraction_directory)
  if(EXISTS "${extraction_directory}")
    return()
  endif()
  file(MAKE_DIRECTORY "${extraction_directory}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xjf "${archive}"
    WORKING_DIRECTORY "${extraction_directory}"
    RESULT_VARIABLE extract_result
    OUTPUT_VARIABLE extract_output
    ERROR_VARIABLE extract_error
  )
  if(NOT extract_result EQUAL 0)
    message(FATAL_ERROR "Could not extract ${archive}: ${extract_error}${extract_output}")
  endif()
endfunction()

function(_meetflow_copy_matching source_root pattern destination)
  if(EXISTS "${destination}")
    message(STATUS "MeetFlow model exists; skipping copy: ${destination}")
    return()
  endif()
  file(GLOB_RECURSE matches CONFIGURE_DEPENDS "${source_root}/${pattern}")
  list(LENGTH matches match_count)
  if(match_count EQUAL 0)
    message(FATAL_ERROR "Downloaded model archive does not contain ${pattern}")
  endif()
  list(GET matches 0 source_file)
  get_filename_component(destination_parent "${destination}" DIRECTORY)
  file(MAKE_DIRECTORY "${destination_parent}")
  file(COPY_FILE "${source_file}" "${destination}")
endfunction()

function(meetflow_prepare_models models_directory)
  set(download_root "${CMAKE_BINARY_DIR}/_model_downloads")
  set(whisper_archive "${download_root}/whisper.tar.bz2")
  set(segmentation_archive "${download_root}/segmentation.tar.bz2")
  set(whisper_extract "${download_root}/whisper")
  set(segmentation_extract "${download_root}/segmentation")

  set(whisper_encoder "${models_directory}/whisper/encoder.int8.onnx")
  set(whisper_decoder "${models_directory}/whisper/decoder.int8.onnx")
  set(whisper_tokens "${models_directory}/whisper/tokens.txt")
  set(segmentation_model "${models_directory}/diarization/segmentation.onnx")
  set(embedding_model "${models_directory}/diarization/embedding.onnx")

  if(NOT EXISTS "${whisper_encoder}" OR NOT EXISTS "${whisper_decoder}" OR NOT EXISTS "${whisper_tokens}")
    _meetflow_download("${MEETFLOW_WHISPER_MODEL_URL}" "${whisper_archive}")
    _meetflow_extract_archive("${whisper_archive}" "${whisper_extract}")
    _meetflow_copy_matching("${whisper_extract}" "*encoder.int8.onnx" "${whisper_encoder}")
    _meetflow_copy_matching("${whisper_extract}" "*decoder.int8.onnx" "${whisper_decoder}")
    _meetflow_copy_matching("${whisper_extract}" "*tokens.txt" "${whisper_tokens}")
  endif()

  if(NOT EXISTS "${segmentation_model}")
    _meetflow_download("${MEETFLOW_DIARIZATION_SEGMENTATION_URL}" "${segmentation_archive}")
    _meetflow_extract_archive("${segmentation_archive}" "${segmentation_extract}")
    _meetflow_copy_matching("${segmentation_extract}" "model.onnx" "${segmentation_model}")
  endif()

  _meetflow_download("${MEETFLOW_DIARIZATION_EMBEDDING_URL}" "${embedding_model}")
  message(STATUS "MeetFlow models are ready in ${models_directory}")
endfunction()
