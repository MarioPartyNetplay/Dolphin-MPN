// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WiiBanner.h"

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/NandPaths.h"

#include "Common/Swap.h"
#include "Core/IOS/WFS/WFSI.h"

#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
constexpr u32 BANNER_WIDTH = 192;
constexpr u32 BANNER_HEIGHT = 64;
constexpr u32 BANNER_SIZE = BANNER_WIDTH * BANNER_HEIGHT * 2;

constexpr u32 ICON_WIDTH = 48;
constexpr u32 ICON_HEIGHT = 48;
constexpr u32 ICON_SIZE = ICON_WIDTH * ICON_HEIGHT * 2;

WiiBanner::WiiBanner(u64 title_id)
{
  m_path = (Common::GetTitleDataPath(title_id, Common::FromWhichRoot::Configured) + "/opening.bnr");
  ExtractARC(m_path);
}

WiiBanner::WiiBanner(const Volume& volume, Partition partition)
{
  m_path = (File::CreateTempDir() + "/opening.bnr");
  if (ExportFile(volume, partition, "opening.bnr", m_path))
    ExtractARC(m_path);
  else
    m_valid = false;
}

void WiiBanner::ExtractARC(std::string path)
{
  IOS::HLE::ARCUnpacker m_arc_unpacker;
  File::IOFile file(path, "rb");
  std::vector<u8> bytes;
  const u32 offset = 0x0600;

  if (!file)
  {
    m_valid = false;
    return;
  }

  file.Seek(offset, File::SeekOrigin::Begin);

  u32 length = file.GetSize() - offset;
  bytes.resize(length);
  file.ReadBytes(bytes.data(), length);

  m_arc_unpacker.AddBytes(bytes);

  const std::string outdir = File::CreateTempDir();

  const auto callback = [=](const std::string& filename, const std::vector<u8>& outbytes) {
    const std::string outpath = (outdir + "/" + filename);
    File::CreateFullPath(outpath);
    File::IOFile outfile(outpath, "wb");
    outfile.WriteBytes(outbytes.data(), outbytes.size());
  };

  m_arc_unpacker.Extract(callback);

  ExtractBin(outdir + "/meta/banner.bin");
}

void WiiBanner::ExtractBin(std::string path)
{
  IOS::HLE::ARCUnpacker m_arc_unpacker;
  File::IOFile file(path, "rb");
  std::vector<u8> bytes;
  const u32 offset = 0x20;

  if (!file)
  {
    m_valid = false;
    return;
  }

  file.Seek(offset, File::SeekOrigin::Begin);

  const u32 length = file.GetSize() - offset;
  bytes.resize(length);
  file.ReadBytes(bytes.data(), length);

  u32 header = Common::swap32(bytes.data());

  if (header == 0x4C5A3737)  // "LZ77"
  {
    bytes.resize(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16));
    bytes = DecompressLZ77(bytes);
    header = Common::swap32(bytes.data());
  }

  if (header != 0x55AA382D)
  {
    m_valid = false;
    return;
  }

  m_arc_unpacker.AddBytes(bytes);

  const std::string outdir = File::CreateTempDir();

  const auto callback = [=](const std::string& filename, const std::vector<u8>& outbytes) {
    const std::string outpath = (outdir + "/" + filename);
    File::CreateFullPath(outpath);
    File::IOFile outfile(outpath, "wb");
    outfile.WriteBytes(outbytes.data(), outbytes.size());
  };

  m_arc_unpacker.Extract(callback);
}

std::vector<u8> WiiBanner::DecompressLZ77(std::vector<u8> bytes)
{
  std::vector<u8> output;

  u32 uncompressed_length;
  std::memcpy(&uncompressed_length, &bytes[4], sizeof(u32));

  u8 compression_method = static_cast<uint8_t>(uncompressed_length & 0xFF);

  uncompressed_length >>= 8;

  if (compression_method != 0x10)
  {
    return output;
  }

  // output.resize(uncompressed_length);

  size_t pos = 8;
  u32 written = 0;
  while (written != uncompressed_length)
  {
    u8 flags = bytes[pos++];
    for (int f = 0; f != 8; ++f)
    {
      if (flags & 0x80)  // If the bit is set, it's a reference
      {
        u16 info = (bytes[pos] << 8) | bytes[pos + 1];
        pos += 2;

        const u8 num = 3 + (info >> 12);
        const u16 disp = info & 0xFFF;
        u32 ptr = written - disp - 1;

        for (u8 p = 0; p != num; ++p)
        {
          u8 c = output[ptr + p];
          output.push_back(c);
          ++written;

          if (written == uncompressed_length)
            break;
        }
      }
      else
      {
        u8 c = bytes[pos++];
        output.push_back(c);
        ++written;
      }

      flags <<= 1;

      if (written == uncompressed_length)
        break;
    }
  }

  return output;
}

std::string WiiBanner::GetName() const
{
  // In the wii save banner.bin the name is from 0x20 to 0x3F
  // In the opening.bnr the name is SOMETIMES from 0x5C to 0x7B
  // of course the current extaction is also currently broken (see: the smurfs dance party)
  // but this wont be any more broken
  // ok well not exactly
  // it will be broken in a different way
  // return UTF16BEToUTF8(m_header.name, std::size(m_header.name));
  return std::string();
}

std::vector<u32> WiiBanner::GetBanner(u32* width, u32* height) const
{
  return std::vector<u32>();
  // *width = 0;
  // *height = 0;

  // File::IOFile file(m_path, "rb");
  // if (!file.Seek(sizeof(Header), File::SeekOrigin::Begin))
  //   return std::vector<u32>();

  // std::vector<u16> banner_data(BANNER_WIDTH * BANNER_HEIGHT);
  // if (!file.ReadArray(banner_data.data(), banner_data.size()))
  //   return std::vector<u32>();

  // std::vector<u32> image_buffer(BANNER_WIDTH * BANNER_HEIGHT);
  // Common::Decode5A3Image(image_buffer.data(), banner_data.data(), BANNER_WIDTH, BANNER_HEIGHT);

  // *width = BANNER_WIDTH;
  // *height = BANNER_HEIGHT;
  // return image_buffer;
}

std::string WiiBanner::GetDescription() const
{
  return std::string();
}

// const auto callback = [this](const std::string& filename, const std::vector<u8>& bytes)
// {
//   INFO_LOG_FMT(IOS_WFS, "Extract: {} ({} bytes)", filename, bytes.size());

//   const std::string path = WFS::NativePath(m_base_extract_path + '/' + filename);
//   File::CreateFullPath(path);
//   File::IOFile f(path, "wb");
//   if (!f)
//   {
//     ERROR_LOG_FMT(IOS_WFS, "Could not extract {} to {}", filename, path);
//     return;
//   }
//   f.WriteBytes(bytes.data(), bytes.size());
// };
// m_arc_unpacker.Extract(callback);

// bool WiiBanner::ParseIMET(std::string path)
// {
//   constexpr u32 IMET_MAGIC = 0x494D4554;
//   u32 magic;

//   File::IOFile file(path, "rb");

//   if (file.GetSize() < (BANNER_SIZE + ICON_SIZE))
//     m_valid = false;

//   file.Seek(40, File::SeekOrigin::Begin);
//   file.ReadBytes(&magic, 4);
//   if (magic == IMET_MAGIC)
//     ParseArc(path);
//   else if (magic != IMET_MAGIC)
//     m_valid = false;

//   return 0;
// }

// bool WiiBanner::ParseARC(std::string path)
// {
//   U8Header header;
//   u32 magic;

//   File::IOFile file(path, "rb");

//   file.Seek(600, File::SeekOrigin::Current);
//   file.ReadBytes(&magic, 4);
//   if (magic != header.magic)
//     m_valid = false;

//   return 0;
// }

}  // namespace DiscIO
