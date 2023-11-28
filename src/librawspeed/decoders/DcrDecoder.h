/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#pragma once

#include "common/RawImage.h"
#include "decoders/SimpleTiffDecoder.h"
#include "io/Buffer.h"
#include "tiff/TiffIFD.h"
#include <utility>

namespace rawspeed {

class Buffer;
class CameraMetaData;

class DcrDecoder final : public SimpleTiffDecoder {
  void checkImageDimensions() final;

public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD, Buffer file);
  DcrDecoder(TiffRootIFDOwner&& root, Buffer file)
      : SimpleTiffDecoder(std::move(root), file) {}

  RawImage decodeRawInternal() final;
  void decodeMetaDataInternal(const CameraMetaData* meta) final;

private:
  [[nodiscard]] int getDecoderVersion() const final { return 0; }
};

} // namespace rawspeed
