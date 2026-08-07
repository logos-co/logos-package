#pragma once

// Shared PNG fixture builder for tests that need a package icon.
//
// Package::validateIconAsset() only reads IHDR (width/height at fixed
// offsets) and never decodes pixels, but we emit a real zlib-compressed
// IDAT so fixtures are decodable images rather than header-shaped stubs —
// otherwise a test could pass against a validator that later starts
// decoding, and we would not find out here.

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <zlib.h>

namespace lgx_test {

// Default size is the 256x256 the Logos icon standard requires; pass another
// size to exercise rejection.
inline std::vector<uint8_t> makePng(uint32_t w = 256, uint32_t h = 256) {
    auto be32 = [](uint32_t v) {
        return std::vector<uint8_t>{
            static_cast<uint8_t>((v >> 24) & 0xFF),
            static_cast<uint8_t>((v >> 16) & 0xFF),
            static_cast<uint8_t>((v >> 8) & 0xFF),
            static_cast<uint8_t>(v & 0xFF)};
    };
    auto chunk = [&](const std::string& tag,
                     const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> out = be32(static_cast<uint32_t>(payload.size()));
        std::vector<uint8_t> tagged(tag.begin(), tag.end());
        tagged.insert(tagged.end(), payload.begin(), payload.end());
        out.insert(out.end(), tagged.begin(), tagged.end());
        const uint32_t crc = static_cast<uint32_t>(
            ::crc32(0L, tagged.data(), static_cast<uInt>(tagged.size())));
        const auto crcBytes = be32(crc);
        out.insert(out.end(), crcBytes.begin(), crcBytes.end());
        return out;
    };

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    std::vector<uint8_t> ihdr = be32(w);
    const auto hh = be32(h);
    ihdr.insert(ihdr.end(), hh.begin(), hh.end());
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA
    const auto ihdrChunk = chunk("IHDR", ihdr);
    png.insert(png.end(), ihdrChunk.begin(), ihdrChunk.end());

    // One filter byte + 4 bytes/px per row, all zero (transparent black).
    std::vector<uint8_t> raw(static_cast<size_t>(h) * (1 + 4 * w), 0);
    uLongf destLen = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> deflated(destLen);
    // Return an empty fixture on compression failure rather than emitting a
    // PNG with a truncated IDAT — callers fail fast (setIcon() rejects empty
    // data), so the real cause surfaces instead of a confusing decode error.
    if (compress2(deflated.data(), &destLen, raw.data(),
                  static_cast<uLong>(raw.size()), 9) != Z_OK) {
        return {};
    }
    deflated.resize(destLen);
    const auto idat = chunk("IDAT", deflated);
    png.insert(png.end(), idat.begin(), idat.end());

    const auto iend = chunk("IEND", {});
    png.insert(png.end(), iend.begin(), iend.end());
    return png;
}

// Write a PNG fixture to disk and return the path, for CLI tests that pass
// --icon rather than calling Package::setIcon() directly.
inline std::string writePng(const std::string& path,
                            uint32_t w = 256,
                            uint32_t h = 256) {
    const auto data = makePng(w, h);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return path;
}

} // namespace lgx_test
