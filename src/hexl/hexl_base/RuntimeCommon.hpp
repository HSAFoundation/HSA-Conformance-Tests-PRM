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

#ifndef HEXL_RUNTIME_COMMON_HPP
#define HEXL_RUNTIME_COMMON_HPP

#include "MObject.hpp"
#include <vector>
#include <thread>
#include "Brig.h"

namespace hexl {
  class Context;

  class ImageParams {
  public:
    BrigType imageType;
    BrigImageGeometry geometry;
    BrigImageChannelOrder channelOrder;
    BrigImageChannelType channelType;
    size_t width;
    size_t height;
    size_t depth;
    size_t arraySize;

    ImageParams(
      BrigType imageType_,
      BrigImageGeometry geometry_,
      BrigImageChannelOrder channelOrder_,
      BrigImageChannelType channelType_,
      size_t width_,
      size_t height_,
      size_t depth_,
      size_t arraySize_)
      : imageType(imageType_),
        geometry(geometry_), channelOrder(channelOrder_), channelType(channelType_),
        width(width_), height(height_), depth(depth_), arraySize(arraySize_) { }
    ImageParams() { }
    void Print(std::ostream& out) const;
  };

  class SamplerParams {
  private:
    BrigSamplerCoordNormalization coord;
    BrigSamplerFilter filter;
    BrigSamplerAddressing addressing;

  public:
    SamplerParams(
      BrigSamplerCoordNormalization coord_,
      BrigSamplerFilter filter_,
      BrigSamplerAddressing addressing_)
      : coord(coord_),
        filter(filter_),
        addressing(addressing_) { }
    SamplerParams() { }

    bool IsValid() const;

    BrigSamplerCoordNormalization Coord() const { return coord; }
    BrigSamplerFilter Filter() const { return filter; }
    BrigSamplerAddressing Addressing() const { return addressing; }
  
    void Coord(BrigSamplerCoordNormalization coord_) { coord = coord_; }
    void Filter(BrigSamplerFilter filter_) { filter = filter_; }
    void Addressing(BrigSamplerAddressing addressing_) { addressing = addressing_; }

    void Print(std::ostream& out) const;
    void Name(std::ostream& out) const;
  };

  inline std::ostream& operator<<(std::ostream& out, const SamplerParams& params) { params.Name(out); return out; }

  class ImageRegion {
  public:
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t size_x;
    uint32_t size_y;
    uint32_t size_z;

    ImageRegion(uint32_t x_ = 0, uint32_t y_ = 0, uint32_t z_ = 0,
                uint32_t size_x_ = 1, uint32_t size_y_ = 1, uint32_t size_z_ = 1)
                : x(x_), y(y_), z(z_), size_x(size_x_), size_y(size_y_), size_z(size_z_) {}
    void Print(std::ostream& out) const;
  };


  namespace runtime {

    class RuntimeState;

    class Command {
    public:
      virtual ~Command() { }

      static Command* CreateFromString(const std::string& s);

      virtual void Print(std::ostream& out) const = 0;
      virtual bool Execute(runtime::RuntimeState* runtime) = 0;
      virtual bool Finish(runtime::RuntimeState* runtime) { return true; }
    };

    enum DispatchArgType {
      DARG_VALUE,
      DARG_VALUES,
      DARG_BUFFER,
      DARG_IMAGE,
      DARG_SAMPLER,
      DARG_SIGNAL,
      DARG_QUEUE,
      DARG_GROUPOFFSET
    };

    class RuntimeState {
    private:
      unsigned argNum;

    public:
      RuntimeState() : argNum(0) { }
      virtual ~RuntimeState() { }

      virtual void Print(std::ostream& out) const { }
      
      virtual Context* GetContext() = 0;

      virtual void Set(const std::string& key, Value value);
      virtual void Set(const std::string& parent, const std::string& key, Value value);
      virtual Value Get(const std::string& key);
      virtual Value Get(const std::string& parent, const std::string& key);

      virtual bool StartThread(unsigned id, Command* commandToRun) = 0;
      virtual bool WaitThreads() = 0;

      virtual bool ModuleCreateFromBrig(const std::string& moduleId = "module", const std::string& brigId = "brig") = 0;

      virtual bool ProgramCreate(const std::string& programId = "program") = 0;
      virtual bool ProgramAddModule(const std::string& programId = "program", const std::string& moduleId = "module") = 0;
      virtual bool ProgramFinalize(const std::string& codeId = "code", const std::string& programId = "program") = 0;

      virtual bool ExecutableCreate(const std::string& executableId = "executable") = 0;
      virtual bool ExecutableLoadCode(const std::string& executableId = "executable", const std::string& codeId = "code") = 0;
      virtual bool ExecutableFreeze(const std::string& executableId = "executable") = 0;

      virtual bool BufferCreate(const std::string& bufferId, size_t size, const std::string& initValuesId = "") = 0;
      virtual bool BufferValidate(const std::string& bufferId, const std::string& expectedValuesId, const std::string& method = "") = 0;

      virtual bool ImageCreate(const std::string& imageId, const std::string& imageParamsId) = 0;
      virtual bool ImageInitialize(const std::string& imageId, const std::string& imageParamsId, const std::string& initValueId) = 0;
      virtual bool ImageWrite(const std::string& imageId, const std::string& writeValuesId, const ImageRegion& region) = 0;
      virtual bool ImageValidate(const std::string& imageId, const std::string& expectedValuesId, const std::string& method = "") = 0;
      virtual bool SamplerCreate(const std::string& samplerId, const std::string& samplerParamsId) = 0;

      virtual bool DispatchCreate(const std::string& dispatchId = "dispatch", const std::string& executableId = "executable", const std::string& kernelName = "") = 0;
      virtual bool DispatchArg(const std::string& dispatchId, DispatchArgType argType, const std::string& argKey) = 0;
      bool DispatchValueArg(const std::string& dispatchId, Value value);
      bool DispatchValuesArg(const std::string& dispatchId, Values* values);
      bool DispatchGroupOffsetArg(const std::string& dispatchId, Value value = Value(MV_UINT32, 0));
      virtual bool DispatchExecute(const std::string& dispatchId = "dispatch") = 0;

      virtual bool SignalCreate(const std::string& signalId, uint64_t signalInitialValue = 1) = 0;
      virtual bool SignalSend(const std::string& signalId, uint64_t signalSendValue = 1) = 0;
      virtual bool SignalWait(const std::string& signalId, uint64_t signalExpectedValue = 1) = 0;

      virtual bool QueueCreate(const std::string& queueId, uint32_t size = 0) = 0;

      virtual bool IsDetectSupported() = 0;
      virtual bool IsBreakSupported() = 0;
    };

    class RuntimeContext {
    protected:
      Context* context;

    public:
      RuntimeContext(Context* context_)
        : context(context_) { }
      virtual ~RuntimeContext() { }
    
      virtual void Print(std::ostream& out) const { out << Description(); }
      virtual bool Init() = 0;
      virtual RuntimeState* NewState(Context* context) = 0;
      virtual std::string Description() const = 0;
      virtual bool IsFullProfile() = 0;
      virtual uint32_t Wavesize()= 0;
      virtual uint32_t WavesPerGroup() = 0;
      virtual bool IsLittleEndianness() { return true; };
    };

    class HostThreads {
    private:
      RuntimeState* rt;
      std::vector<bool> results;
      std::vector<std::thread> threads;

      void RunThread(unsigned id, Command* command);
      bool Result(unsigned id) const;

    public:
      HostThreads(RuntimeState* rt_)
        : rt(rt_) { }
      bool StartThread(unsigned id, Command* command);
      bool WaitThreads();
    };
  }

}

#endif // HEXL_RUNTIME_COMMON_HPP
