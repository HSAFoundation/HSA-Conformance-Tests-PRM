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

#include "Grid.hpp"

namespace hexl {

void Dim::Name(std::ostream& out) const
{
  out << data[0] << 'x' << data[1] << 'x' << data[2];
}

void GridGeometry::Name(std::ostream& out) const
{
  out << nDim << "_" << gridSize << "_" << workgroupSize;
}

void GridGeometry::Print(std::ostream& out) const
{
    out << "Dimensions: " << nDim << std::endl
        << "Grid:       " << "(" << gridSize[0] << ", " << gridSize[1] << ", " << gridSize[2] << ")" << std::endl
        << "Workgroup:  " << "(" << workgroupSize[0] << ", " << workgroupSize[1] << ", " << workgroupSize[2] << ")" << std::endl;
//        << "Offsets:    " << "(" << globalOffset[0] << ", " << globalOffset[1] << ", " << globalOffset[2] << ")" << std::endl;
}

uint32_t GridGeometry::WorkitemId(Dim point, uint16_t dim) const
{
  switch(dim) {
  case 0:
    return point[X] %  workgroupSize[X];
  case 1:
    return dim < nDim ? point[Y] % workgroupSize[Y] : 0;
  case 2:
    return dim < nDim ? point[Z] % workgroupSize[Z] : 0;
  default:
    assert(false);
  }
  return 0;
}

uint64_t GridGeometry::WorkitemAbsId(Dim point, uint16_t dim) const
{
  switch(dim) {
  case 0:
    return point[X] %  gridSize[X];
  case 1:
    return dim < nDim ? point[Y] % gridSize[Y] : 0;
  case 2:
    return dim < nDim ? point[Z] % gridSize[Z] : 0;
  default:
    assert(false);
  }
  return 0;
}
  
uint32_t GridGeometry::WorkitemFlatId(Dim point) const
{
  return WorkitemId(point, 0) + WorkitemId(point, 1) * workgroupSize[X] + WorkitemId(point, 2) * workgroupSize[X] * workgroupSize[Y];
}
  
uint64_t GridGeometry::WorkitemFlatAbsId(Dim point) const
{
  return WorkitemAbsId(point, 0) + WorkitemAbsId(point, 1) * gridSize[X] + WorkitemAbsId(point, 2) * gridSize[X] * gridSize[Y];
}
  
uint32_t GridGeometry::WorkgroupId(Dim point, uint16_t dim) const
{
  switch(dim) {
  case 0:
    return point.get(dim) / workgroupSize[X];
  case 1:
    return dim < nDim ? point.get(dim) / workgroupSize[Y] : 0;
  case 2:
    return dim < nDim ? point.get(dim) / workgroupSize[Z] : 0;
  default:
    assert(false);
  }
  return 0;
}

uint32_t GridGeometry::WorkgroupFlatId (Dim point) const 
{
  return WorkgroupId(point, 0) 
       + WorkgroupId(point, 1) * GridGroups(0) 
       + WorkgroupId(point, 2) * GridGroups(0) * GridGroups(1);
}

uint32_t GridGeometry::GridGroups(uint16_t dim) const
{
  switch(dim) {
  case 0:
    return (gridSize[X]+workgroupSize[X]-1)/workgroupSize[X];
  case 1:
    return dim < nDim ? (gridSize[Y]+workgroupSize[Y]-1)/workgroupSize[Y] : 1;
  case 2:
    return dim < nDim ? (gridSize[Z]+workgroupSize[Z]-1)/workgroupSize[Z] : 1;
  default:
    assert(false);
  }
  return 0;
}
  
uint32_t GridGeometry::CurrentWorkgroupSize(Dim point) const
{
  return CurrentWorkgroupSize(point, 0) * 
          CurrentWorkgroupSize(point, 1) * 
          CurrentWorkgroupSize(point, 2);
}

uint32_t GridGeometry::CurrentWorkgroupSize(Dim point, uint16_t dim) const 
{
  uint32_t whole = 0, partial = 0;
  switch(dim) {
  case 0:
    whole = gridSize[X]/workgroupSize[X];
    partial =  gridSize[X]%workgroupSize[X];
    return point[X] < (whole*workgroupSize[X]) ? workgroupSize[X] : partial;
  case 1:
    if (dim < nDim)
    {
      whole = gridSize[Y]/workgroupSize[Y];
      partial =  gridSize[Y]%workgroupSize[Y];
      return point[Y] < (whole*workgroupSize[Y]) ? workgroupSize[Y] : partial;
    }
    return 1;
  case 2:
    if (dim < nDim)
    {
      whole = gridSize[Z]/workgroupSize[Z];
      partial =  gridSize[Z]%workgroupSize[Z];
      return point[Z] < (whole*workgroupSize[Z]) ? workgroupSize[Z] : partial;
    }
    return 1;
  default:
    assert(false);
  }
  return 1;
}

Dim GridGeometry::Point(uint64_t flatabsid) const
{
  uint64_t z = flatabsid / (gridSize.get(0) * gridSize.get(1));
  uint64_t y = (flatabsid - z * (gridSize.get(0) * gridSize.get(1))) / gridSize.get(0);
  uint64_t x = flatabsid - z * (gridSize.get(0) * gridSize.get(1)) - y * gridSize.get(0);
  return Dim(x, y, z);
}

uint64_t GridGeometry::WaveNum(Dim point, uint32_t waveSize) const
{
  return WorkitemFlatAbsId(point) / waveSize;
}

}
