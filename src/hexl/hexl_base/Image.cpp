/*
   Copyright 2014 Heterogeneous System Architecture (HSA) Foundation

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

#include "Image.hpp"

namespace hexl {

void ImageDim::Name(std::ostream& out) const
{
  out << data[0] << 'x' << data[1] << 'x' << data[2] << "x[p" << data[3] << "s" << data[4] << ']';
}

void ImageGeometry::Name(std::ostream& out) const
{
  out << imageSize;
}

void ImageGeometry::Print(std::ostream& out) const
{
    out << "Image:       " << "(" << imageSize[0] << ", " << imageSize[1] << ", " << imageSize[2] << ")" << " [pitch: " << imageSize[3] << ", slice: " << imageSize[4] << ']' << std::endl;
}

ImageDim ImageIterator::operator*()
{
  return point;
}

ImageIterator& ImageIterator::operator++()
{
  unsigned x = point.get(0), y = point.get(1), z = point.get(2), pitch = point.get(3), slice = point.get(4);
  unsigned gsx = geometry->ImageSize(0), gsy = geometry->ImageSize(1);
  x++;
  if (x == gsx) {
    y++; x = 0;
    if (y == gsy) {
      z++; y = 0;
    }
  }
  point = ImageDim(x, y, z, pitch, slice);
  return *this;
}

ImageIterator ImageGeometry::ImageBegin() const
{
  return ImageIterator(this, 0, 0, 0);
}

ImageIterator ImageGeometry::ImageEnd() const
{
  return ImageIterator(this, 0, 0, ImageSize(2));
}

bool ImageIterator::operator!=(const ImageIterator& i)
{
  return (geometry != i.geometry) || (point != i.point);
}

}
