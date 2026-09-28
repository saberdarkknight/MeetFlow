#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_SIZE="${1:-small}"
MODEL_DIR="${PROJECT_ROOT}/model/${MODEL_SIZE}"

case "${MODEL_SIZE}" in
  tiny|base|small) ;;
  *)
    echo "Usage: $0 [tiny|base|small]" >&2
    exit 2
    ;;
esac

cd "${PROJECT_ROOT}"

cmake -S . -B build \
  -DMEETFLOW_ENABLE_SHERPA_ONNX=ON \
  -DMEETFLOW_SHERPA_ONNX_ROOT="${PROJECT_ROOT}/.third_party/sherpa-onnx" \
  -DMEETFLOW_WHISPER_MODEL_SIZE="${MODEL_SIZE}" \
  -DMEETFLOW_DOWNLOAD_MODELS=ON \
  -DMEETFLOW_MODELS_DIR="${MODEL_DIR}"

cmake --build build --parallel

echo "MeetFlow build complete (Whisper model: ${MODEL_SIZE})"
echo "Use --models \"${MODEL_DIR}\" when running meetflow-prepare."
