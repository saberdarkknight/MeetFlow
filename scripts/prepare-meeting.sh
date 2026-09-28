#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -lt 1 || $# -gt 4 ]]; then
  echo "Usage: $0 <audio-file> [output-directory] [model-size] [max-speakers]" >&2
  echo "Example: $0 Audio/meeting_0916.m4a build/output small 6" >&2
  exit 2
fi

AUDIO_FILE="$1"
OUTPUT_DIR="${2:-${PROJECT_ROOT}/build/output}"
MODEL_SIZE="${3:-small}"
MAX_SPEAKERS="${4:-0}"
MODEL_DIR="${PROJECT_ROOT}/model/${MODEL_SIZE}"
PREPARE_BINARY="${PROJECT_ROOT}/build/bin/meetflow-prepare"

case "${MODEL_SIZE}" in
  tiny|base|small) ;;
  *) echo "Model size must be tiny, base, or small: ${MODEL_SIZE}" >&2; exit 2 ;;
esac
if [[ ! -f "${AUDIO_FILE}" ]]; then
  echo "Audio file does not exist: ${AUDIO_FILE}" >&2
  exit 1
fi
if [[ ! -x "${PREPARE_BINARY}" ]]; then
  echo "${PREPARE_BINARY} was not found. Run ./scripts/build-meetflow.sh ${MODEL_SIZE} first." >&2
  exit 1
fi
if [[ ! -d "${MODEL_DIR}" ]]; then
  echo "Model directory does not exist: ${MODEL_DIR}" >&2
  echo "Run ./scripts/build-meetflow.sh ${MODEL_SIZE} first." >&2
  exit 1
fi
if ! [[ "${MAX_SPEAKERS}" =~ ^[0-9]+$ ]]; then
  echo "Maximum speakers must be a non-negative integer: ${MAX_SPEAKERS}" >&2
  exit 2
fi

cd "${PROJECT_ROOT}"
echo "Preparing ${AUDIO_FILE}"
echo "Model: ${MODEL_SIZE}; maximum speakers: ${MAX_SPEAKERS:-automatic}"
exec "${PREPARE_BINARY}" "${AUDIO_FILE}" \
  --output "${OUTPUT_DIR}" \
  --models "${MODEL_DIR}" \
  --max-speakers "${MAX_SPEAKERS}"
