/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Vasily Khoruzhick

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "rawspeedconfig.h"

#ifdef HAVE_ZLIB

#include "common/FloatingPoint.h"         // for fp16ToFloat, fp24ToFloat
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/DeflateDecompressor.h"
#include "io/Endianness.h" // for getHostEndianness, Endianness
#include <cassert>         // for assert
#include <cstdint>         // for uint32_t, uint16_t
#include <cstdio>          // for size_t
#include <zlib.h>          // for uncompress, zError, Z_OK

namespace rawspeed {

DeflateDecompressor::DeflateDecompressor(ByteStream bs, const RawImage& img,
                                         int predictor, int bps_)
    : input(std::move(bs)), mRaw(img), bps(bps_) {
  switch (predictor) {
  case 3:
    predFactor = 1;
    break;
  case 34894:
    predFactor = 2;
    break;
  case 34895:
    predFactor = 4;
    break;
  default:
    ThrowRDE("Unsupported predictor %i", predictor);
  }
  predFactor *= mRaw->getCpp();
}

// decodeFPDeltaRow(): MIT License, copyright 2014 Javier Celaya
// <jcelaya@gmail.com>
static inline void decodeDeltaBytes(unsigned char* src, size_t realTileWidth,
                                    unsigned int bytesps, int factor) {
  for (size_t col = factor; col < realTileWidth * bytesps; ++col) {
    // Yes, this is correct, and is symmetrical with EncodeDeltaBytes in
    // hdrmerge, and they both combined are lossless.
    // This is indeed working in modulo-2^n arighmetics.
    src[col] = static_cast<unsigned char>(src[col] + src[col - factor]);
  }
}

template <typename T> struct StorageType {};
template <> struct StorageType<ieee_754_2008::Binary16> {
  using type = uint16_t;
  static constexpr int padding_bytes = 0;
};
template <> struct StorageType<ieee_754_2008::Binary24> {
  using type = uint32_t;
  static constexpr int padding_bytes = 1;
};
template <> struct StorageType<ieee_754_2008::Binary32> {
  using type = uint32_t;
  static constexpr int padding_bytes = 0;
};

template <typename T>
static inline void decodeFPDeltaRow(unsigned char* src, unsigned char* dst,
                                    size_t tileWidth, size_t realTileWidth) {
  using storage_type = typename StorageType<T>::type;
  constexpr unsigned storage_bytes = sizeof(storage_type);
  constexpr unsigned bytesps = T::StorageWidth / 8;
  auto* dst32 = reinterpret_cast<uint32_t*>(dst);

  for (size_t col = 0; col < tileWidth; ++col) {
    std::array<unsigned char, storage_bytes> bytes;
    for (int c = 0; c != bytesps; ++c)
      bytes[c] = src[col + c * realTileWidth];

    auto tmp = getBE<storage_type>(bytes.data());
    tmp >>= CHAR_BIT * StorageType<T>::padding_bytes;

    uint32_t tmp_expanded;
    switch (bytesps) {
    case 2:
    case 3:
      tmp_expanded = extendBinaryFloatingPoint<T, ieee_754_2008::Binary32>(tmp);
      break;
    case 4:
      tmp_expanded = tmp;
      break;
    }

    dst32[col] = tmp_expanded;
  }
}

void DeflateDecompressor::decode(
    std::unique_ptr<unsigned char[]>* uBuffer, // NOLINT
    iPoint2D maxDim, iPoint2D dim, iPoint2D off) {
  uLongf dstLen = sizeof(float) * maxDim.area();

  if (!*uBuffer)
    *uBuffer =
        std::unique_ptr<unsigned char[]>(new unsigned char[dstLen]); // NOLINT

  const auto cSize = input.getRemainSize();
  const unsigned char* cBuffer = input.getData(cSize);

  if (int err = uncompress(uBuffer->get(), &dstLen, cBuffer, cSize);
      err != Z_OK) {
    ThrowRDE("failed to uncompress tile: %d (%s)", err, zError(err));
  }

  int bytesps = bps / 8;
  assert(bytesps >= 2 && bytesps <= 4);

  for (auto row = 0; row < dim.y; ++row) {
    unsigned char* src = uBuffer->get() + row * maxDim.x * bytesps;
    unsigned char* dst =
        mRaw->getData() + ((off.y + row) * mRaw->pitch + off.x * sizeof(float));

    decodeDeltaBytes(src, maxDim.x, bytesps, predFactor);

    switch (bytesps) {
    case 2:
      decodeFPDeltaRow<ieee_754_2008::Binary16>(src, dst, dim.x, maxDim.x);
      break;
    case 3:
      decodeFPDeltaRow<ieee_754_2008::Binary24>(src, dst, dim.x, maxDim.x);
      break;
    case 4:
      decodeFPDeltaRow<ieee_754_2008::Binary32>(src, dst, dim.x, maxDim.x);
      break;
    }
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "ZLIB is not present! Deflate compression will not be supported!"

#endif
