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

#ifndef HEXL_GRID_HPP
#define HEXL_GRID_HPP

#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstdio>
#include "Arena.hpp"

namespace hexl {

  class Dim {
  private:
    uint64_t data[3];

  public:
    Dim() { data[0]=1; data[1]=1; data[2]=1; }
    explicit Dim(uint32_t p[3]) { data[0]=p[0]; data[1]=p[1]; data[2]=p[2]; }
    Dim(uint64_t x, uint64_t y=1, uint64_t z=1) { data[0]=x; data[1]=y; data[2]=z; }
    uint32_t operator[](uint16_t idx) const { return get(idx); }
    uint32_t get(uint16_t idx) const { assert(data[idx] <= UINT32_MAX); return (uint32_t) data[idx]; }
    uint64_t get64(uint16_t idx) const { return data[idx]; }
    uint64_t size() const { return data[0] * data[1] * data[2]; }
    uint32_t size32() const { uint64_t s = size(); assert(s <= UINT32_MAX); return (uint32_t) s; }
    bool operator==(const Dim& p) { return data[0] == p.data[0] && data[1] == p.data[1] && data[2] == p.data[2]; }
    bool operator!=(const Dim& p) { return data[0] != p.data[0] || data[1] != p.data[1] || data[2] != p.data[2]; }
    void Name(std::ostream& o) const;
  };

  inline std::ostream& operator<<(std::ostream& o, const Dim& dim) { dim.Name(o); return o; }

  class GridGeometry;

  class GridIterator {
  private:
    const GridGeometry* geometry;
    Dim point;

  public:
    GridIterator(const GridGeometry* geometry_, const Dim& point_)
      : geometry(geometry_), point(point_) { }
    GridIterator(const GridGeometry* geometry_, uint64_t x, uint64_t y, uint64_t z)
      : geometry(geometry_), point(x, y, z) { }
    const Dim& operator*();
    GridIterator& operator++();
    bool operator!=(const GridIterator& i);
  };

  class WorkgroupIterator {
  private:
    const GridGeometry* geometry;
    Dim point;

  public:
    WorkgroupIterator(const GridGeometry* geometry_, const Dim& point_)
      : geometry(geometry_), point(point_) { }
    Dim operator*();
    WorkgroupIterator& operator++();
    bool operator!=(const WorkgroupIterator& i);
  };

  class GridGeometry {
    ARENAORHEAPMEM
    static const uint16_t maxDim = 3;
    enum { X, Y, Z };
    uint32_t nDim;
    Dim gridSize;
    Dim workgroupSize;

  public:
    explicit GridGeometry(uint32_t ndim = 1, const Dim& gs = Dim(), const Dim& ws = Dim()) : nDim(ndim), gridSize(gs), workgroupSize(ws) { assert(nDim <= maxDim); }
    explicit GridGeometry(uint32_t geometry[7]) : nDim(geometry[0]), gridSize(&geometry[1]), workgroupSize(&geometry[4]) { assert(nDim <= maxDim); }
    explicit GridGeometry(uint32_t ndim, uint32_t *gs, uint32_t *ws) : nDim(ndim), gridSize(gs), workgroupSize(ws) { assert(nDim <= maxDim); }
    explicit GridGeometry(uint32_t ndim, uint32_t gridSizeX, uint32_t gridSizeY, uint32_t gridSizeZ, uint32_t workgroupSizeX, uint32_t workgroupSizeY, uint32_t workgroupSizeZ)
      : nDim(ndim),
        gridSize(gridSizeX, gridSizeY, gridSizeZ),
        workgroupSize(workgroupSizeX, workgroupSizeY, workgroupSizeZ) { }

    void Name(std::ostream& out) const;
    void Print(std::ostream& out) const;

    uint32_t NDim() const { return nDim; }
    uint32_t Dimensions() const { return nDim; }

    bool isPartial() const {
      return (((gridSize[X]%workgroupSize[X]) > 0) || ((gridSize[Y]%workgroupSize[Y]) > 0) || ((gridSize[Z]%workgroupSize[Z]) > 0));
    }

    uint64_t GridSize() const { return gridSize.size(); }
    uint32_t GridSize32() const { return gridSize.size32(); }
    uint32_t GridSize32(uint16_t dim) const { return gridSize.get(dim); }
    uint32_t GridSize(uint16_t dim) const { return gridSize.get(dim); }
    uint32_t WorkgroupSize() const { return workgroupSize.size32(); }
    uint32_t WorkgroupSize(uint16_t dim) const { return workgroupSize.get(dim); }
    uint32_t WorkitemId(Dim point, uint16_t dim) const;
    uint32_t WorkgroupId (Dim point, uint16_t dim) const;
    uint32_t WorkgroupFlatId (Dim point) const;
    uint64_t WorkitemAbsId(Dim point, uint16_t dim) const;
    uint32_t WorkitemFlatId(Dim point) const;
    uint32_t WorkitemCurrentFlatId (Dim point) const;
    uint64_t WorkitemFlatAbsId(const Dim& point) const;
    uint32_t GridGroups(uint16_t dim) const;
    uint32_t GridGroups() const;
    uint32_t CurrentWorkgroupSize(Dim point) const;
    uint32_t CurrentWorkgroupSize(Dim point, uint16_t dim) const;
    Dim Point(uint64_t workitemflatabsid) const;
    Dim Point(uint32_t workgroupflatid, uint32_t workitemflatid) const;
    GridIterator GridBegin() const;
    GridIterator GridEnd() const;
    WorkgroupIterator WaveBegin(uint32_t wavenum) const;
    WorkgroupIterator WaveEnd(uint32_t wavenum) const;
    // TODO: Move WaveSize from CoreConfig to here
    uint32_t LaneId(const Dim& point, uint32_t wavesize) const;
    uint32_t WaveNumInWorkgroup(const Dim& point, uint32_t wavesize) const;
    uint32_t MaxWaveNumInWorkgroup(uint32_t wavesize) const;
    uint32_t WaveIndex(const Dim& point, uint32_t wavesize) const;
    uint32_t MaxWaveIndex(uint32_t wavesize) const;
    uint64_t WaveCount(uint32_t wavesize) const;
    uint64_t WaveNum(Dim point, uint32_t waveSize) const;
  };

  typedef const GridGeometry* Grid;
}

inline std::ostream& operator<<(std::ostream& out, const hexl::GridGeometry& geometry) { geometry.Name(out); return out; }
inline std::ostream& operator<<(std::ostream& out, hexl::Grid grid) { grid->Name(out); return out; }

#endif // HEXL_GRID_HPP
