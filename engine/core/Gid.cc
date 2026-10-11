/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "core/Gid.h"

#include <spdlog/fmt/fmt.h>

#include <charconv>
#include <random>

namespace aria2 {

Gid newGid()
{
  static std::mt19937_64 random{std::random_device{}()};
  Gid gid = 0;
  while (gid == 0) {
    gid = random();
  }
  return gid;
}

std::string gidText(Gid gid) { return fmt::format("{:016x}", gid); }

std::optional<Gid> parseGid(std::string_view text)
{
  Gid gid = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), gid, 16);
  if (text.size() != 16 || error != std::errc() ||
      end != text.data() + text.size() || gid == 0) {
    return std::nullopt;
  }
  return gid;
}

} // namespace aria2
