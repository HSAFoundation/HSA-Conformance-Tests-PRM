/*
   Copyright 2014-2015 Heterogeneous System Architecture (HSA) Foundation

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef HEXL_IMAGE_HPP
#define HEXL_IMAGE_HPP

#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstdio>
#include "Arena.hpp"
#include "Brig.h"

namespace hexl {

  class ImageDim {
  private:
    union {
      unsigned data[4];
      struct {
        unsigned imageX, imageY, imageZ, imageArray;
      };
    };
  public:
    ImageDim() { imageX=1; imageY=1; imageZ=1; imageArray=1;}

    explicit ImageDim(unsigned p[6]) { data[0]=p[0]; data[1]=p[1]; data[2]=p[2]; data[3]=p[3];}
    ImageDim(unsigned x, unsigned y=1, unsigned z=1, unsigned array=1) { data[0]=x; data[1]=y; data[2]=z; data[3]=array; }
    unsigned operator[](uint16_t idx) const { return get(idx); }
    unsigned get(unsigned idx) const { assert(data[idx] <= UINT32_MAX); return data[idx]; }
    uint64_t size() const { return data[0] * data[1] * data[2] * data[3]; }
    unsigned size32() const { uint64_t s = size(); assert(s <= UINT32_MAX); return (unsigned) s; }
    bool operator==(const ImageDim& p) { return data[0] == p.data[0] && data[1] == p.data[1] && data[2] == p.data[2] && data[3] == p.data[3]; }
    bool operator!=(const ImageDim& p) { return data[0] != p.data[0] || data[1] != p.data[1] || data[2] != p.data[2] || data[3] != p.data[3]; }
    void Name(std::ostream& o) const;
  };

  inline std::ostream& operator<<(std::ostream& o, const ImageDim& dim) { dim.Name(o); return o; }

  class ImageGeometry;

  class ImageIterator {
  private:
    const ImageGeometry* geometry;
    ImageDim point;
  public:
    ImageIterator(const ImageGeometry* geometry_, const ImageDim& point_)
      : geometry(geometry_), point(point_) { }
    ImageIterator(const ImageGeometry* geometry_, unsigned x, unsigned y, unsigned z, unsigned array = 1)
      : geometry(geometry_), point(x, y, z, array) { }
    ImageDim operator*();
    ImageIterator& operator++();
    bool operator!=(const ImageIterator& i);
  };

  class ImageGeometry {
    ARENAORHEAPMEM
    static const uint16_t maxDim = 3;
    enum { X, Y, Z, Array };
    ImageDim imageSize;
  public:
    explicit ImageGeometry(unsigned x = 1, unsigned y = 1, unsigned z = 1, unsigned array = 1) : imageSize(x,y,z,array) {  }
    explicit ImageGeometry(const ImageDim& is) : imageSize(is) {  }

    void Name(std::ostream& out) const;
    void Print(std::ostream& out) const;

    uint64_t ImageSize() const { return imageSize.size(); }
    uint32_t ImageSize32() const { return imageSize.size32(); }
    uint32_t ImageSize32(uint16_t dim) const { return imageSize.get(dim); }
    uint32_t ImageSize(uint16_t dim) const { return imageSize.get(dim); }
    uint32_t ImageWidth() const { return imageSize.get(X); }
    uint32_t ImageHeight() const { return imageSize.get(Y); }
    uint32_t ImageDepth() const { return imageSize.get(Z); }
    uint32_t ImageArray() const { return imageSize.get(Array); }
    ImageIterator ImageBegin() const;
    ImageIterator ImageEnd() const;
  };
}

inline std::ostream& operator<<(std::ostream& out, const hexl::ImageGeometry& geometry) { geometry.Name(out); return out; }
inline std::ostream& operator<<(std::ostream& out, hexl::ImageGeometry* image) { image->Name(out); return out; }

#endif // HEXL_IMAGE_HPP
