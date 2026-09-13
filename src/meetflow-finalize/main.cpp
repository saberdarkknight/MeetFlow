#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kName = "meetflow-finalize";

void print_usage() {
  std::cout << "Usage: " << kName << " <meeting-directory>\n"
            << "\n"
            << "Render approved Markdown minutes from reviewed action items.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    print_usage();
    return 0;
  }

  std::cerr << kName << ": not implemented yet. Run with --help for usage.\n";
  return 1;
}

