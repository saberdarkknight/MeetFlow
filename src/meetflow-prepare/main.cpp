#include <CommonCrypto/CommonDigest.h>
#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kName = "meetflow-prepare";
constexpr UInt32 kTargetSampleRate = 16'000;
constexpr UInt32 kChunkDurationSeconds = 10 * 60;

struct Options {
  std::filesystem::path audio_file;
  std::filesystem::path output_directory;
};

void print_usage() {
  std::cout << "Usage: " << kName << " <audio-file> [--output <meeting-directory>]\n"
            << "\n"
            << "Import an audio recording and create local source artifacts for a meeting.\n"
            << "\n"
            << "Without --output, artifacts are created under "
               "./meetflow-meetings/meeting-<UTC timestamp>.\n";
}

std::optional<Options> parse_options(int argc, char* argv[]) {
  if (argc < 2) {
    return std::nullopt;
  }

  Options options{.audio_file = argv[1]};
  for (int index = 2; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--output") {
      if (++index == argc) {
        throw std::runtime_error("--output requires a directory path");
      }
      options.output_directory = argv[index];
      continue;
    }
    throw std::runtime_error("unknown argument: " + std::string{argument});
  }

  return options;
}

std::string timestamp_utc() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  gmtime_r(&time, &utc);

  std::ostringstream timestamp;
  timestamp << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
  return timestamp.str();
}

std::string json_escape(std::string_view value) {
  std::ostringstream escaped;
  for (const unsigned char character : value) {
    switch (character) {
      case '"': escaped << "\\\""; break;
      case '\\': escaped << "\\\\"; break;
      case '\b': escaped << "\\b"; break;
      case '\f': escaped << "\\f"; break;
      case '\n': escaped << "\\n"; break;
      case '\r': escaped << "\\r"; break;
      case '\t': escaped << "\\t"; break;
      default:
        if (character < 0x20) {
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<unsigned int>(character) << std::dec;
        } else {
          escaped << character;
        }
    }
  }
  return escaped.str();
}

std::string sha256_file(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error("cannot open file for hashing: " + path.string());
  }

  CC_SHA256_CTX context;
  CC_SHA256_Init(&context);

  char buffer[64 * 1024];
  while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
    CC_SHA256_Update(&context, buffer, static_cast<CC_LONG>(input.gcount()));
  }
  if (!input.eof()) {
    throw std::runtime_error("failed while hashing: " + path.string());
  }

  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256_Final(digest, &context);

  std::ostringstream hex;
  for (const unsigned char byte : digest) {
    hex << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<unsigned int>(byte);
  }
  return hex.str();
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  if (!output) {
    throw std::runtime_error("cannot create file: " + path.string());
  }
  output << contents;
  if (!output) {
    throw std::runtime_error("failed while writing: " + path.string());
  }
}

std::filesystem::path default_output_directory(std::string_view timestamp) {
  return std::filesystem::current_path() / "meetflow-meetings" /
         ("meeting-" + std::string{timestamp});
}

std::string os_status_message(std::string_view operation, OSStatus status) {
  return std::string{operation} + " failed (OSStatus " + std::to_string(status) + ")";
}

void write_u32_le(std::array<char, 44>& header, std::size_t offset, std::uint32_t value) {
  for (std::size_t byte = 0; byte < 4; ++byte) {
    header[offset + byte] = static_cast<char>((value >> (byte * 8)) & 0xffU);
  }
}

void write_u16_le(std::array<char, 44>& header, std::size_t offset, std::uint16_t value) {
  for (std::size_t byte = 0; byte < 2; ++byte) {
    header[offset + byte] = static_cast<char>((value >> (byte * 8)) & 0xffU);
  }
}

std::array<char, 44> wave_header(std::uint32_t data_bytes) {
  std::array<char, 44> header{};
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  write_u32_le(header, 4, 36U + data_bytes);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  write_u32_le(header, 16, 16);
  write_u16_le(header, 20, 1);
  write_u16_le(header, 22, 1);
  write_u32_le(header, 24, kTargetSampleRate);
  write_u32_le(header, 28, kTargetSampleRate * 2U);
  write_u16_le(header, 32, 2);
  write_u16_le(header, 34, 16);
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  write_u32_le(header, 40, data_bytes);
  return header;
}

std::uint64_t decode_to_mono_wave(const std::filesystem::path& input_path,
                                  const std::filesystem::path& output_path) {
  const std::string input_utf8 = input_path.string();
  CFURLRef input_url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(input_utf8.data()),
      static_cast<CFIndex>(input_utf8.size()), false);
  if (input_url == nullptr) {
    throw std::runtime_error("cannot create a file URL for: " + input_path.string());
  }

  ExtAudioFileRef input_file = nullptr;
  const OSStatus open_status = ExtAudioFileOpenURL(input_url, &input_file);
  CFRelease(input_url);
  if (open_status != noErr) {
    throw std::runtime_error(os_status_message("opening audio", open_status));
  }

  try {
    AudioStreamBasicDescription pcm{};
    pcm.mSampleRate = kTargetSampleRate;
    pcm.mFormatID = kAudioFormatLinearPCM;
    pcm.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
                       kAudioFormatFlagsNativeEndian;
    pcm.mBitsPerChannel = 32;
    pcm.mChannelsPerFrame = 1;
    pcm.mFramesPerPacket = 1;
    pcm.mBytesPerFrame = 4;
    pcm.mBytesPerPacket = 4;

    const OSStatus format_status = ExtAudioFileSetProperty(
        input_file, kExtAudioFileProperty_ClientDataFormat, sizeof(pcm), &pcm);
    if (format_status != noErr) {
      throw std::runtime_error(os_status_message("configuring PCM audio", format_status));
    }

    std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
    if (!output) {
      throw std::runtime_error("cannot create normalized audio: " + output_path.string());
    }
    output.write(std::array<char, 44>{}.data(), 44);

    constexpr UInt32 kFramesPerRead = 16'384;
    std::vector<float> buffer(kFramesPerRead);
    std::vector<std::int16_t> pcm_buffer(kFramesPerRead);
    std::uint64_t total_frames = 0;
    while (true) {
      AudioBufferList buffers{};
      buffers.mNumberBuffers = 1;
      buffers.mBuffers[0].mNumberChannels = 1;
      buffers.mBuffers[0].mDataByteSize =
          static_cast<UInt32>(buffer.size() * sizeof(float));
      buffers.mBuffers[0].mData = buffer.data();
      UInt32 frames = kFramesPerRead;
      const OSStatus read_status = ExtAudioFileRead(input_file, &frames, &buffers);
      if (read_status != noErr) {
        throw std::runtime_error(os_status_message("decoding audio", read_status));
      }
      if (frames == 0) {
        break;
      }
      for (UInt32 index = 0; index < frames; ++index) {
        const float sample = std::clamp(buffer[index], -1.0F, 1.0F);
        pcm_buffer[index] = sample <= -1.0F
                                ? std::numeric_limits<std::int16_t>::min()
                                : static_cast<std::int16_t>(sample * 32767.0F);
      }
      output.write(reinterpret_cast<const char*>(pcm_buffer.data()),
                   static_cast<std::streamsize>(frames * sizeof(std::int16_t)));
      if (!output) {
        throw std::runtime_error("failed while writing normalized audio");
      }
      total_frames += frames;
    }

    if (total_frames > (std::numeric_limits<std::uint32_t>::max() - 36U) / 2U) {
      throw std::runtime_error("normalized audio exceeds the WAV size limit");
    }
    output.seekp(0);
    const auto header = wave_header(static_cast<std::uint32_t>(total_frames * 2U));
    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    if (!output) {
      throw std::runtime_error("failed while finalizing normalized audio");
    }

    ExtAudioFileDispose(input_file);
    return total_frames;
  } catch (...) {
    ExtAudioFileDispose(input_file);
    throw;
  }
}

std::string processing_chunks_json(std::uint64_t total_frames) {
  const std::uint64_t chunk_frames = kTargetSampleRate * kChunkDurationSeconds;
  std::ostringstream chunks;
  chunks << "{\n  \"schema_version\": 1,\n"
         << "  \"source\": \"audio/normalized-16k-mono.wav\",\n"
         << "  \"sample_rate_hz\": " << kTargetSampleRate << ",\n"
         << "  \"chunks\": [";
  for (std::uint64_t start = 0, index = 1; start < total_frames; start += chunk_frames, ++index) {
    const std::uint64_t end = std::min(start + chunk_frames, total_frames);
    chunks << (start == 0 ? "\n" : ",\n")
           << "    {\n"
           << "      \"id\": \"chunk_" << std::setw(4) << std::setfill('0') << index
           << std::setfill(' ') << "\",\n"
           << "      \"start_seconds\": " << std::fixed << std::setprecision(3)
           << static_cast<double>(start) / kTargetSampleRate << ",\n"
           << "      \"end_seconds\": " << static_cast<double>(end) / kTargetSampleRate << "\n"
           << "    }";
  }
  chunks << (total_frames == 0 ? "" : "\n") << "  ]\n}\n";
  return chunks.str();
}

void create_artifacts(const Options& options) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(options.audio_file, error) || error) {
    throw std::runtime_error("audio file does not exist or is not a regular file: " +
                             options.audio_file.string());
  }

  const std::string created_at = timestamp_utc();
  const auto destination = options.output_directory.empty()
                               ? default_output_directory(created_at)
                               : options.output_directory;
  if (std::filesystem::exists(destination)) {
    throw std::runtime_error("output directory already exists: " + destination.string());
  }

  std::filesystem::create_directories(destination);
  const auto source_name = "original" + options.audio_file.extension().string();
  const auto imported_audio = destination / source_name;

  try {
    std::filesystem::copy_file(options.audio_file, imported_audio);
    std::filesystem::create_directories(destination / "audio");
    const std::uint64_t normalized_frames = decode_to_mono_wave(
        imported_audio, destination / "audio" / "normalized-16k-mono.wav");
    const std::string checksum = sha256_file(imported_audio);
    const std::string meeting_id = "meeting-" + created_at + "-" + checksum.substr(0, 8);

    std::ostringstream manifest;
    manifest << "{\n"
             << "  \"schema_version\": 1,\n"
             << "  \"meeting_id\": \"" << meeting_id << "\",\n"
             << "  \"created_at_utc\": \"" << created_at << "\",\n"
             << "  \"source_audio\": {\n"
             << "    \"file\": \"" << json_escape(source_name) << "\",\n"
             << "    \"sha256\": \"" << checksum << "\",\n"
             << "    \"bytes\": " << std::filesystem::file_size(imported_audio) << "\n"
             << "  },\n"
             << "  \"artifacts\": {\n"
             << "    \"transcript\": \"transcript.json\",\n"
             << "    \"diarization\": \"diarization.json\",\n"
             << "    \"processing_chunks\": \"processing-chunks.json\",\n"
             << "    \"speaker_review\": \"speakers.yaml\"\n"
             << "  }\n"
             << "}\n";

    write_file(destination / "manifest.json", manifest.str());
    write_file(destination / "transcript.json",
               "{\n  \"schema_version\": 1,\n  \"segments\": []\n}\n");
    write_file(destination / "diarization.json",
               "{\n  \"schema_version\": 1,\n  \"speakers\": [],\n  \"segments\": []\n}\n");
    write_file(destination / "processing-chunks.json", processing_chunks_json(normalized_frames));
    write_file(destination / "speakers.yaml",
               "# Add names after reviewing diarization output.\n"
               "speaker_names: {}\n"
               "segment_overrides: {}\n");
  } catch (...) {
    std::filesystem::remove_all(destination, error);
    throw;
  }

  std::cout << "Created meeting workspace: " << destination << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    print_usage();
    return 0;
  }

  try {
    const auto options = parse_options(argc, argv);
    if (!options) {
      print_usage();
      return 1;
    }
    create_artifacts(*options);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << kName << ": " << error.what() << "\n";
    return 1;
  }
}
