add_library(discio
  Blob.cpp
  Blob.h
  CISOBlob.cpp
  CISOBlob.h
  CompressedBlob.cpp
  CompressedBlob.h
  DirectoryBlob.cpp
  DirectoryBlob.h
  DiscExtractor.cpp
  DiscExtractor.h
  DiscScrubber.cpp
  DiscScrubber.h
  DiscUtils.cpp
  DiscUtils.h
  Enums.cpp
  Enums.h
  FileBlob.cpp
  FileBlob.h
  FileSystemGCWii.cpp
  FileSystemGCWii.h
  Filesystem.cpp
  Filesystem.h
  GameModDescriptor.cpp
  GameModDescriptor.h
  LaggedFibonacciGenerator.cpp
  LaggedFibonacciGenerator.h
  MultithreadedCompressor.h
  NANDImporter.cpp
  NANDImporter.h
  NFSBlob.cpp
  NFSBlob.h
  RiivolutionParser.cpp
  RiivolutionParser.h
  RiivolutionPatcher.cpp
  RiivolutionPatcher.h
  ScrubbedBlob.cpp
  ScrubbedBlob.h
  SplitFileBlob.cpp
  SplitFileBlob.h
  TGCBlob.cpp
  TGCBlob.h
  Volume.cpp
  Volume.h
  VolumeDisc.cpp
  VolumeDisc.h
  VolumeFileBlobReader.cpp
  VolumeFileBlobReader.h
  VolumeGC.cpp
  VolumeGC.h
  VolumeVerifier.cpp
  VolumeVerifier.h
  VolumeWad.cpp
  VolumeWad.h
  VolumeWii.cpp
  VolumeWii.h
  WbfsBlob.cpp
  WbfsBlob.h
  WIABlob.cpp
  WIABlob.h
  WIACompression.cpp
  WIACompression.h
  WiiBanner.cpp
  WiiBanner.h
  WiiEncryptionCache.cpp
  WiiEncryptionCache.h

)

target_link_libraries(discio
PUBLIC
  BZip2::BZip2
  LibLZMA::LibLZMA
  zstd::zstd

PRIVATE
  fmt::fmt
  minizip-ng::minizip-ng
  pugixml
  ZLIB::ZLIB
)

# Link to core PRIVATE to avoid circular dependency propagation, but we need it for ARCUnpacker
# The circular dependency (core <-> discio) is handled by CMake's dependency resolution
target_link_libraries(discio PRIVATE core)

if(MSVC)
  # Add precompiled header
  target_link_libraries(discio PRIVATE use_pch)
endif()
