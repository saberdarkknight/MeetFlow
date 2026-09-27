#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${PROJECT_ROOT}/onnx"
BUILD_DIR="${SOURCE_DIR}/build"
INSTALL_DIR="${PROJECT_ROOT}/.third_party/sherpa-onnx"

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
  if [[ -e "${SOURCE_DIR}" ]] && [[ -n "$(find "${SOURCE_DIR}" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
    echo "${SOURCE_DIR} exists but is not a Sherpa-ONNX checkout." >&2
    echo "Move or remove that directory, then run this script again." >&2
    exit 1
  fi
  echo "Sherpa-ONNX source not found; cloning v1.13.8 into ${SOURCE_DIR}..."
  mkdir -p "${PROJECT_ROOT}"
  git clone --depth 1 --branch v1.13.8 \
    https://github.com/k2-fsa/sherpa-onnx.git "${SOURCE_DIR}"
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_BINARY=OFF \
  -DSHERPA_ONNX_ENABLE_CHECK=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_TTS=OFF \
  -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
  -DSHERPA_ONNX_BUILD_C_API_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"

cmake --build "${BUILD_DIR}" --parallel
cmake --install "${BUILD_DIR}"

echo "Sherpa-ONNX installed in ${INSTALL_DIR}"
