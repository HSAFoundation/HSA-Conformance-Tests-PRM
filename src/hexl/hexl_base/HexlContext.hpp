#ifndef HEXL_CONTEXT_HPP
#define HEXL_CONTEXT_HPP

#include <map>
#include <string>
#include <ostream>
#include "MObject.hpp"
#include "HexlObjects.hpp"

namespace hexl {

  class ContextObject {
  public:
    virtual ~ContextObject() { }
    virtual void Print(std::ostream& out) const = 0;
    virtual void Dump(const std::string& path, const std::string& name) const = 0;
  };

  template <typename T>
  class ContextPointer : public ContextObject {
  protected:
    T* t;
    ContextPointer(T* t_) : t(t_) { }

  public:
    T* Get() { return t; }
    void Print(std::ostream& out) const override { hexl::Print(*t, out); }
    void Dump(const std::string& path, const std::string& name) const override { hexl::Dump(*t, path, name); }
  };

  template <>
  class ContextPointer<void> : public ContextObject {
  protected:
    void* t;
    ContextPointer(void* t_) : t(t_) { }

  public:
    void* Get() { return t; }
    void Print(std::ostream& out) const { out << "<void>"; }
    void Dump(const std::string& path, const std::string& name) const override { }
  };

  template <typename T>
  class ContextUnmanagedPointer : public ContextPointer<T> {
  public:
    ContextUnmanagedPointer(T* t_) : ContextPointer<T>(t_) { }
  };

  template <typename T>
  class ContextManagedPointer : public ContextPointer<T> {
  public:
    ContextManagedPointer(T* t_) : ContextPointer<T>(t_) { }
    ~ContextManagedPointer() { delete this->t;  }
  };

  template <typename T>
  class ContextValue : public ContextObject {
  private:
    T value;

  public:
    ContextValue(const T& value_)
      : value(value_) { }

    const T& Get() { return value; }
    void Print(std::ostream& out) const { hexl::Print<T>(value, out); }
    void Dump(const std::string& path, const std::string& name) const override { hexl::Dump(value, path, name); }
  };

  class Context {
  private:
    Context* parent;
    std::map<std::string, std::unique_ptr<ContextObject>> map;

    void PutObject(const std::string& key, ContextObject* o)
    {
      map[key] = std::unique_ptr<ContextObject>(o);
    }

    template <typename T>
    T* GetObject(const std::string& key) const
    {
      auto f = map.find(key);
      if (f != map.end()) {
        return static_cast<T*>(f->second.get());
      }
      else {
        if (parent) {
          return parent->GetObject<T>(key);
        }
        else {
          std::cout << "Key: " << key << std::endl;
          assert(!"Value not found");
          return 0;
        }
      }
    }

  public:
    explicit Context(Context* parent_ = 0)
      : parent(parent_) { }

    void SetParent(Context* parent) { this->parent = parent; }

    void Print(std::ostream& out) const;

    void Dump() const;

    bool Has(const std::string& key) const { return map.find(key) != map.end(); }
    bool Has(const std::string& path, const std::string& key) const { return Has(path + "." + key); }

    void Clear() { map.clear(); }

    void Put(const std::string& key, const Value& value) { PutObject(key, new ContextValue<Value>(value)); }
    void Put(const std::string& path, const std::string& key, const Value& value) { Put(path + "." + key, value); }
    const Value& GetValue(const std::string& key) const { return GetObject<ContextValue<Value>>(key)->Get(); }
    const Value& GetValue(const std::string& path, const std::string& key) const { return GetValue(path + "." + key); }

    void Put(const std::string& key, uint64_t handle) { Put(key, Value(MV_UINT64, handle)); }
    uint64_t GetHandle(const std::string& key) const { return GetValue(key).U64(); }

    void Put(const std::string& key, const Values& values) { PutObject(key, new ContextValue<Values>(values)); }
    void Move(const std::string& key, Values& values);
    const Values& GetValues(const std::string& key) const { return GetObject<ContextValue<Values>>(key)->Get(); }

    void Put(const std::string& key, const std::string& s) { PutObject(key, new ContextValue<std::string>(s)); }
    void Put(const std::string& path, const std::string& key, const std::string& s) { Put(path + "." + key, s); }
    const std::string& GetString(const std::string& path, const std::string& key) const { return GetString(path + "." + key); }
    const std::string& GetString(const std::string& key) const { return GetObject<ContextValue<std::string>>(key)->Get(); }

    template <typename T>
    void Put(const std::string& key, T* t) { PutObject(key, new ContextUnmanagedPointer<T>(t)); }
    template <typename T>
    void Move(const std::string& key, T* t) { PutObject(key, new ContextManagedPointer<T>(t)); }
    template<class T>
    T* Get(const std::string& key) { return GetObject<ContextPointer<T>>(key)->Get(); }
    template<class T>
    const T* Get(const std::string& key) const { return GetObject<ContextPointer<T>>(key)->Get(); }

    Value GetRuntimeValue(Value v);

    void Delete(const std::string& key) { map.erase(key); }

    // Logging helpers.
    std::ostream& Debug() { return *Get<std::ostream>("hexl.log.stream.debug"); }
    std::ostream& Info() { return *Get<std::ostream>("hexl.log.stream.info"); }
    std::ostream& Error() { return *Get<std::ostream>("hexl.log.stream.error"); }

#ifdef _WIN32
    void Win32Error(const std::string& msg = "");
#endif // _WIN32

    bool IsLarge() const { return sizeof(void *) == 8; }
    bool IsVerbose(const std::string& what, bool enabledWithPlainVerboseOption = true) const;
    template <typename T>
    void PrintIfVerbose(const std::string& name, const std::string& desc, const T& t) {
      if (IsVerbose(name)) {
        Debug() << desc << ":" << std::endl;
        IndentStream indent(Debug());
        t->Print(Debug());
      }
    }
    bool IsDumpEnabled(const std::string& what, bool enableWithPlainDumpOption = true) const;
    void SetOutputPath(const std::string& path) { Put("hexl.outputPath", path); }
    const std::string& GetOutputPath() const { return GetString("hexl.outputPath"); }
    std::string GetOutputName(const std::string& name, const std::string& what);
    bool DumpTextIfEnabled(const std::string& name, const std::string& what, const std::string& text);
    bool DumpBinaryIfEnabled(const std::string& name, const std::string& what, const void *buffer, size_t bufferSize);
    void DumpBrigIfEnabled(const std::string& name, HSAIL_ASM::BrigContainer* brig);

    // Helper methods. Include HexlTest.hpp to use them.
    ResourceManager* RM() { return Get<ResourceManager>("hexl.rm"); }
    const ResourceManager* RM() const { return Get<ResourceManager>("hexl.rm"); }
    TestFactory* Factory() { return Get<TestFactory>("hexl.testFactory"); }
    //runtime::RuntimeState* State() { return Get<runtime::RuntimeState>("hexl.runtimestate"); }
    runtime::RuntimeContext* Runtime() { return Get<runtime::RuntimeContext>("hexl.runtime"); }
    const Options* Opts() const { return Get<Options>("hexl.options"); }
    AllStats& Stats() { return *Get<AllStats>("hexl.stats"); }
  };

  bool ValidateMemory(Context* context, const Values& expected, const void *actualPtr, const std::string& method);
}

#endif // HEXL_CONTEXT_HPP
