# Local inference setup

MeetFlow will use [Sherpa-ONNX v1.13.8](https://github.com/k2-fsa/sherpa-onnx/releases/tag/v1.13.8) as its native C++ runtime for `meetflow-prepare`.

The default build does not require Sherpa-ONNX. This allows work on the CLI, meeting schemas, and deterministic artifact handling without downloading large model/runtime files.

## Build with Sherpa-ONNX

Build and install Sherpa-ONNX into a local prefix, then configure MeetFlow with that prefix:

```sh
cmake -S . -B build \
  -DMEETFLOW_ENABLE_SHERPA_ONNX=ON \
  -DMEETFLOW_SHERPA_ONNX_ROOT=/absolute/path/to/sherpa-onnx-install
cmake --build build --parallel
```

The prefix must contain:

```text
include/cxx-api.h
lib/libsherpa-onnx-c-api.dylib
```

Do not commit the runtime or model files to this repository.

## Model boundary

The runtime and models are intentionally separate:

- **ASR:** a multilingual Whisper-family ONNX model
- **Diarization:** a speaker-segmentation model plus a speaker-embedding model
- **Language identification:** a local model or ASR-derived result

`meetflow-prepare` will receive a model directory explicitly. The application will record model names, versions, and checksums in each meeting manifest so that reprocessing is reproducible.

No audio, transcript, or derived meeting content is sent to a model host or cloud service.
