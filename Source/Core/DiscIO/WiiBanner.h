// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
class WiiBanner
{
public:
  const static int BANNER_WIDTH = 0;
  const static int BANNER_HEIGHT = 0;

  WiiBanner(u64 title_id);
  WiiBanner(const Volume& volume, Partition partition);

  bool IsValid() const { return m_valid; }
  std::string GetName() const;
  std::string GetDescription() const;

  std::vector<u32> GetBanner(u32* width, u32* height) const;

private:
  std::vector<u8> DecompressLZ77(std::vector<u8> bytes);
  void ExtractARC();
  void ExtractBin(const std::vector<u8>& data);

  bool m_valid = true;
  std::vector<u8> m_bytes;
};

}  // namespace DiscIO
