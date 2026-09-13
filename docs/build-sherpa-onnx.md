# Build Sherpa-ONNX for MeetFlow

MeetFlow uses Sherpa-ONNX as an optional, local native runtime for transcription and speaker diarization. This guide builds version `v1.13.8` on Apple Silicon macOS without Python.

The runtime and its models are local dependencies. Do not add them to this Git repository.

## Prerequisites

- Apple Silicon Mac
- Xcode Command Line Tools
- CMake 3.25 or newer
- Git and network access for the initial source/dependency download

## Build and install

Choose a local location outside this repository. This example installs into `~/Library/Application Support/MeetFlow/runtime/sherpa-onnx`.

```sh
git clone --depth 1 --branch v1.13.8 https://github.com/k2-fsa/sherpa-onnx.git ~/src/sherpa-onnx-v1.13.8

cmake -S ~/src/sherpa-onnx-v1.13.8 -B ~/src/sherpa-onnx-v1.13.8/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DSHERPA_ONNX_ENABLE_TTS=OFF \
  -DSHERPA_ONNX_ENABLE_CHECK=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_BINARY=OFF \
  -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
  -DSHERPA_ONNX_BUILD_C_API_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX="$HOME/Library/Application Support/MeetFlow/runtime/sherpa-onnx"

cmake --build ~/src/sherpa-onnx-v1.13.8/build --parallel
cmake --install ~/src/sherpa-onnx-v1.13.8/build
```

The installed runtime should provide these files:

```text
include/cxx-api.h
lib/libsherpa-onnx-cxx-api.dylib
lib/libsherpa-onnx-c-api.dylib
lib/libonnxruntime.dylib
```

## Build MeetFlow against the runtime

```sh
cmake -S . -B build \
  -DMEETFLOW_ENABLE_SHERPA_ONNX=ON \
  -DMEETFLOW_SHERPA_ONNX_ROOT="$HOME/Library/Application Support/MeetFlow/runtime/sherpa-onnx"
cmake --build build --parallel
```

For development only, `MEETFLOW_SHERPA_ONNX_ROOT` may point to the cloned Sherpa-ONNX source directory. MeetFlow also recognizes its `build/lib` and bundled ONNX Runtime paths. Do not depend on that layout for a packaged application.

## Model files

The runtime alone cannot transcribe or diarize. Download and store model files separately from the Git checkout:

- a multilingual Whisper encoder, decoder, and token file;
- a speaker-segmentation model; and
- a speaker-embedding model.

Record each model's version, license, checksum, and local path. MeetFlow will later record the selected versions in each meeting manifest.

### Let CMake download missing models

MeetFlow can download the official Sherpa-ONNX model archives during CMake configure. The downloader is opt-in, checks the normalized target files first, and skips files that already exist:

```sh
cmake -S . -B build-sherpa \
  -DMEETFLOW_ENABLE_SHERPA_ONNX=ON \
  -DMEETFLOW_SHERPA_ONNX_ROOT="$HOME/Library/Application Support/MeetFlow/runtime/sherpa-onnx" \
  -DMEETFLOW_DOWNLOAD_MODELS=ON \
  -DMEETFLOW_MODELS_DIR="$HOME/Library/Application Support/MeetFlow/models"
cmake --build build-sherpa --parallel
```

The first configure downloads only missing files. Re-running configure with the same model directory does not download them again. To use an internal mirror or a pinned model source, override the URL cache variables:

```sh
cmake -S . -B build-sherpa \
  -DMEETFLOW_DOWNLOAD_MODELS=ON \
  -DMEETFLOW_WHISPER_MODEL_URL="https://example.invalid/whisper.tar.bz2" \
  -DMEETFLOW_DIARIZATION_SEGMENTATION_URL="https://example.invalid/segmentation.tar.bz2" \
  -DMEETFLOW_DIARIZATION_EMBEDDING_URL="https://example.invalid/embedding.onnx"
```

The downloaded directory has this layout:

```text
models/
├── whisper/encoder.int8.onnx
├── whisper/decoder.int8.onnx
├── whisper/tokens.txt
├── diarization/segmentation.onnx
└── diarization/embedding.onnx
```

Pass the same directory to the preparation command:

```sh
./build-sherpa/bin/meetflow-prepare recording.m4a \
  --models "$HOME/Library/Application Support/MeetFlow/models"
```

Keep `MEETFLOW_DOWNLOAD_MODELS=OFF` for offline builds and provide the model directory yourself. Model downloads happen at configure time, not when compiling or running MeetFlow.

## Removing files

After a successful installation, the cloned Sherpa-ONNX source tree and its `build/` directory are disposable. Keep the installed runtime and model directory while you want MeetFlow's inference features to work. Deleting either is safe for the repository, but `meetflow-prepare` will no longer be able to transcribe or diarize until they are restored.
