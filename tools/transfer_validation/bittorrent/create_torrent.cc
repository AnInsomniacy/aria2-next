#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <libtorrent/create_torrent.hpp>

namespace lt = libtorrent;

int main(int argc, char** argv)
{
  if (argc != 3 && argc != 4) {
    std::cerr << "usage: aria2_next_create_torrent INPUT OUTPUT [WEB_SEED]\n";
    return 2;
  }

  const auto input = std::filesystem::absolute(argv[1]);
  const auto output = std::filesystem::absolute(argv[2]);
  const auto flags = lt::create_torrent::v1_only;
  auto files = lt::list_files(
      input.string(), [](const std::string&) { return true; }, flags);
  if (files.empty()) {
    std::cerr << "input file was not discovered\n";
    return 1;
  }

  lt::create_torrent torrent(std::move(files), 256 * 1024, flags);
  lt::set_piece_hashes(torrent, input.parent_path().string());
  torrent.set_creator("Aria2 Next Transfer Validation Suite");
  if (argc == 4) {
    torrent.add_url_seed(argv[3]);
  }
  const auto buffer = torrent.generate_buf();

  std::ofstream stream(output, std::ios::binary | std::ios::trunc);
  stream.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  if (!stream) {
    std::cerr << "failed to write torrent file\n";
    return 1;
  }
  return 0;
}
