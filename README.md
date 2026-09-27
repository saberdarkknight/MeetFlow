# MeetFlow

**A private, local-first meeting-minutes tool for macOS.**

MeetFlow turns an audio recording from an in-person meeting into a reviewable transcript, draft meeting minutes, and confirmed action items—without uploading meeting content to the cloud.

It is initially a personal command-line tool for an Apple Silicon MacBook Air. Record a meeting in Apple Notes on an iPhone, AirDrop the exported audio to your Mac, and process it locally.

> **Project status:** Early development. The local command-line workflow is implemented; transcription and diarization require the optional Sherpa-ONNX runtime and model files.

## Why MeetFlow?

Writing useful meeting minutes takes time, especially after long in-person meetings. MeetFlow keeps the original audio as a local source record and helps turn it into an auditable draft:

- Transcript text with timestamps
- Anonymous speaker segments that you can name and correct
- Draft summary, decisions, and action-item candidates
- A review step before action items become final
- Approved minutes exported as Markdown

The AI proposes; the user confirms. MeetFlow should use `Unknown` rather than inventing a speaker, owner, or due date.

## Privacy model

Meeting data stays on the Mac.

- **Never uploaded:** audio, transcripts, speaker labels, summaries, minutes, and action items
- **Allowed network use:** one-time downloads of application updates and local model files
- **Storage:** app-managed local storage, outside cloud-synced folders by default
- **Security boundary:** v1 relies on the signed-in macOS account and FileVault; it does not add a separate app password or encryption layer
- **Retention:** original audio is preserved so a meeting can be checked or reprocessed after future improvements

The user is responsible for obtaining any required recording consent.

## Intended workflow

```text
Apple Notes recording on iPhone
        │
        └─ AirDrop exported audio (.m4a) to the Mac
                    │
                    ▼
          meetflow-prepare
          transcript + diarization + speakers.yaml
                    │
          Edit speaker names/corrections in speakers.yaml
                    │
                    ▼
           meetflow-draft
           named transcript + draft minutes + action-items.yaml
                    │
          Review and confirm/edit/dismiss action items
                    │
                    ▼
          meetflow-finalize
          approved Markdown minutes
```

MeetFlow is deliberately split into three local executables. This keeps the source audio and human review points separate from LLM-generated drafts, and will let a future GUI reuse the same meeting data.

## Quick start: audio to final minutes

Build the command-line tools:

```sh
cmake -S . -B build
cmake --build build --parallel
```

For local transcription and speaker diarization, follow [the Sherpa-ONNX build guide](docs/build-sherpa-onnx.md), then configure MeetFlow with the native runtime and model downloads enabled:

### Install Sherpa-ONNX inside this project

The helper script automatically creates or uses the project-local `onnx/` source directory and installs the runtime into `.third_party/sherpa-onnx`:

```sh
./scripts/build-sherpa-onnx.sh
```

If `onnx/` does not exist, the script clones the pinned Sherpa-ONNX v1.13.8 source checkout. It then builds the shared C/C++ APIs and installs them locally. The source checkout, build products, and runtime are ignored by Git.

Configure MeetFlow against that local installation:

```sh
cmake -S . -B build-sherpa \
  -DMEETFLOW_ENABLE_SHERPA_ONNX=ON \
  -DMEETFLOW_SHERPA_ONNX_ROOT="$PWD/.third_party/sherpa-onnx" \
  -DMEETFLOW_DOWNLOAD_MODELS=ON
cmake --build build-sherpa --parallel
```

### Use Sherpa-ONNX installed elsewhere

Sherpa-ONNX does not need to be installed inside the repository. Point `MEETFLOW_SHERPA_ONNX_ROOT` at any installation containing `include/` and `lib/`:

```sh
cmake -S . -B build \
  -DMEETFLOW_ENABLE_SHERPA_ONNX=ON \
  -DMEETFLOW_SHERPA_ONNX_ROOT="$HOME/Library/Application Support/MeetFlow/runtime/sherpa-onnx" \
  -DMEETFLOW_DOWNLOAD_MODELS=ON \
  -DMEETFLOW_MODELS_DIR="$PWD/model"
cmake --build build --parallel
```

The runtime directory should contain `include/sherpa-onnx/c-api/cxx-api.h`, `lib/libsherpa-onnx-cxx-api.dylib`, `lib/libsherpa-onnx-c-api.dylib`, and `lib/libonnxruntime.dylib` on macOS.

Prepare an exported iPhone/Apple Notes recording. The original audio is preserved, while a normalized working copy and processing artifacts are created in the meeting directory:

```sh
./build-sherpa/bin/meetflow-prepare \
  "$HOME/Downloads/meeting.m4a" \
  --output "$PWD/meetflow-meetings/meeting-2026-09-13" \
  --models "$PWD/model"
```

If you are only testing the file-import and artifact pipeline without local inference, omit `--models` and use the regular `build/bin/meetflow-prepare` executable.

Review the generated speaker labels and draft:

```sh
./build-sherpa/bin/meetflow-draft \
  "$PWD/meetflow-meetings/meeting-2026-09-13"
```

Edit these files before finalization:

```text
meetflow-meetings/meeting-2026-09-13/speakers.yaml
meetflow-meetings/meeting-2026-09-13/draft/minutes.draft.md
meetflow-meetings/meeting-2026-09-13/draft/action-items.yaml
```

Change reviewed action items from `needs_review` to `confirmed`. Only confirmed items are included in the final output:

```sh
./build-sherpa/bin/meetflow-finalize \
  "$PWD/meetflow-meetings/meeting-2026-09-13"
```

The approved Markdown minutes are written to:

```text
meetflow-meetings/meeting-2026-09-13/final/minutes.md
```

You can open or share that Markdown file without exposing the original audio or intermediate artifacts.

## Command-line executables

| Executable | Purpose | Inputs | Outputs |
| --- | --- | --- | --- |
| `meetflow-prepare` | Create the immutable meeting source artifacts. | Audio recording | Original audio, manifest, timestamped transcript, diarization output, `speakers.yaml` |
| `meetflow-draft` | Generate reviewable minutes from a speaker-reviewed transcript. | Prepared meeting + `speakers.yaml` | Named transcript, draft minutes, `action-items.yaml` |
| `meetflow-finalize` | Produce the approved minutes deterministically. | Reviewed `action-items.yaml` | `minutes.md` |

The prepare stage records the source and processing metadata. Draft and finalize operate on the meeting directory and keep human review explicit: only action items marked `confirmed` are exported to final minutes.

## Meeting workspace layout

```text
meeting-YYYY-MM-DD/
├── original.m4a              # Immutable source recording
├── manifest.json              # Meeting ID, hashes, versions, processing metadata
├── transcript.json            # Timestamped transcript segments
├── diarization.json           # Anonymous speaker time ranges/clusters
├── speakers.yaml              # Human-reviewed speaker names and segment overrides
├── draft/
│   ├── transcript.md          # Transcript rendered with reviewed names
│   ├── minutes.draft.md       # Editable AI-generated draft
│   └── action-items.yaml      # Candidate commitments for human review
└── final/
    └── minutes.md             # Approved Markdown minutes

```

With local inference enabled, `meetflow-prepare` also writes `audio/speakers/S1.wav`, `S2.wav`, and so on. Each file contains the audio grouped by anonymous diarized speaker, so you can listen to the clip while renaming the speaker in `speakers.yaml`.

Transcript segments will use stable IDs, timestamps, an anonymous speaker ID, detected language, text, and confidence. The YAML review files reference those IDs rather than duplicating raw transcript content.

Example `speakers.yaml`:

```yaml
speaker_names:
  S1: Alice
  S2: Bob

segment_overrides:
  seg_0042:
    speaker: Alice
```

Example `action-items.yaml`:

```yaml
action_items:
  - id: action_01
    status: confirmed
    owner: Alice
    action: Send the revised proposal
    due: 2026-09-17
    source_segment: seg_0042

  - id: action_02
    status: needs_review
    owner: unknown
    action: Share budget figures
    due: null
```

Only confirmed items appear in finalized minutes.

## v1 scope

- Personal use on one Mac
- Native C++ command-line executables; no Python runtime
- Apple Silicon baseline: MacBook Air with 16 GB unified memory
- In-person meetings up to two hours, with up to roughly 15 attendees
- Audio exported from Apple Notes and transferred by AirDrop
- Automatic language detection by default; advanced options to force Chinese, English, or auto/mixed recognition
- Transcript remains in the language spoken; no automatic translation by default
- Anonymous speaker labels first, followed by manual naming, merging, and correction
- Local Markdown export; PDF export may be added later
- Resumable processing with persisted intermediate artifacts

Large-room diarization is best effort. Low-confidence assignments should remain anonymous rather than being attributed incorrectly. An external conference microphone is recommended for large rooms.

## Planned local technology

- **C++ / CMake:** application code and build system
- **sherpa-onnx:** local audio workflow including ASR/Whisper-family inference, VAD, language identification, and speaker diarization through native APIs
- **llama.cpp:** local text-generation inference for draft summaries, decisions, and action-item candidates
- **JSON:** internal structured artifacts and manifest data
- **YAML:** concise, human-editable review files
- **Markdown:** canonical approved-minutes export

Models are downloaded once, versioned, checksummed, and removable from local settings. The repository will pin exact supported model versions and licenses before distribution.

## Output format

Final minutes will be Markdown and include:

```markdown
# Meeting title

Date · Duration · Detected language · Transcript version

## Summary

## Decisions

## Confirmed action items

| Owner | Action | Due | Source |
| --- | --- | --- | --- |

## Open questions / unresolved items

## Transcript
```

Sources will refer back to transcript segments and local audio timestamps for review.

## Non-goals for v1

- A GUI, menu-bar app, or iPhone companion app
- Automatic iPhone transfer beyond AirDrop/import
- Cloud sync, remote inference, or content telemetry
- Persistent voice profiles or cross-meeting speaker recognition
- Automatic action-item approval or external task-system integrations
- Perfect speaker identification in noisy or large rooms
- PDF generation

## Roadmap

1. Define the CMake project, CLI contracts, and JSON/YAML schemas.
2. Implement `meetflow-prepare` with locally stored source artifacts.
3. Implement YAML-based speaker review and validation.
4. Implement `meetflow-draft` with local LLM prompting and structured action-item candidates.
5. Implement `meetflow-finalize` and Markdown rendering.
6. Test the workflow with real Apple Notes recordings, then benchmark and improve quality/performance.
7. Add a GUI and PDF export only after the CLI data pipeline is trusted.

## Development status

The repository currently contains the project definition. Contributions and implementation setup will follow the roadmap above.

## References

- [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)
- [llama.cpp](https://github.com/ggml-org/llama.cpp)
- [Apple Notes: record and transcribe audio](https://support.apple.com/guide/iphone/record-and-transcribe-audio-iphbe11247b5/ios)
