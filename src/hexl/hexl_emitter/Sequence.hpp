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

#ifndef HC_SEQUENCE_HPP
#define HC_SEQUENCE_HPP

#include <cassert>
#include <iostream>
#include <cstdint>
#include <vector>
#include "Arena.hpp"

#define NELEM(arr) ((int)(sizeof(arr)/sizeof((arr)[0]))) // Assume < 2M elements. Unsafe, DNU with pointers!

namespace hexl {

  template<typename T>
  class Action {
  public:
    virtual void operator()(const T& item) = 0;
  };

  template <typename T>
  const char *EmptySequenceName();

  template <typename T>
  void PrintSequenceItem(std::ostream& out, const T& item) {
    out << item;
  }

  template <typename T>
  class Sequence {
    ARENAMEM
  private:
    class CountAction : public Action<T> {
    private:
      unsigned count;

    public:
      explicit CountAction()
        : count(0) { }
      void operator()(const T& item) {
        count++;
      }
      unsigned Count() const { return count; }
    };

    class HasAction : public Action<T> {
    private:
      const T& t;
      bool result;

    public:
      HasAction(const T& t_)
        : t(t_), result(false) { }

      void operator()(const T& item) {
        if (item == t) { result = true; }
      }
      bool Result() const { return result; }
    };

    class PrintAction : public Action<T> {
    private:
      std::ostream& out;
      bool first;

    public:
      PrintAction(std::ostream& out_)
        : out(out_), first(true) { }
      ~PrintAction() {
        if (first) {
          // Nothing printed.
          out << EmptySequenceName<T>();
        }
      }

      void operator()(const T& item) {
        if (!first) { out << "_"; }
        PrintSequenceItem(out, item);
        first = false;
      }
    };

  public:
    virtual void Iterate(Action<T>& a) const = 0;

    unsigned Count() const {
      CountAction counter;
      Iterate(counter);
      return counter.Count();
    }

    bool Has(const T& value) const {
      HasAction has(value);
      Iterate(has);
      return has.Result();
    }

    void Print(std::ostream& out) const {
      PrintAction print(out);
      Iterate(print);
    }
  };

  template <typename T>
  std::ostream& operator<<(std::ostream& out, const Sequence<T>& sequence) {
    sequence.Print(out);
    return out;
  }

  template <typename T>
  std::ostream& operator<<(std::ostream& out, const Sequence<T>* sequence) {
    sequence->Print(out);
    return out;
  }

  template <typename T>
  class EmptySequence : public Sequence<T> {
  public:
    void Iterate(Action<T>& a) const { }
  };

  template<typename T>
  class OneValueSequence : public Sequence<T> {
  private:
    T value;

  public:
    explicit OneValueSequence(const T& value_) : value(value_) { }
    void Iterate(Action<T>& a) const { a(value); }
  };

  template<typename T>
  class ArraySequence : public Sequence<T> {
  private:
    const T* values;
    unsigned length;

  protected:
    const T* Values() { return values; }

  public:
    explicit ArraySequence(const T* values_, unsigned length_) : values(values_), length(length_) { }
    void Iterate(Action<T>& a) const {
      for (unsigned i = 0; i < length; ++i) 
        a(values[i]);
    }
  };

  template<typename T>
  class VectorSequence : public Sequence<T> {
  private:
    std::vector<T> values;

  public:
    void Add(const T& t) { values.push_back(t); }

    void Iterate(Action<T>& a) const {
      for (const T& t : values) { a(t); }
    }
  };

  template <typename T>
  class EnumSequence : public ArraySequence<T> {
  private:
    T* values;

  public:
    EnumSequence(T begin, T end)
      : ArraySequence<T>(EnumArray(begin, end), end - begin) {
    }

    static T* EnumArray(T begin, T end) {
      T* arr = new T[end - begin];
      for (unsigned t = begin, i = 0; t != end; t++, i++) { arr[i] = (T) t; }
      return arr;
    }
  };

  template <typename P1, typename P2>
  class Pair {
  private:
    const P1& p1;
    const P2& p2;

    Pair(const Pair&);

  public:
    Pair(const P1& p1_, const P2& p2_)
      : p1(p1_), p2(p2_) { }

    const P1& First() const { return p1; }
    const P2& Second() const { return p2; }
  };

  template <typename P1, typename P2>
  class ApplyPairAction : public Action<P2> {
  private:
    const P1& p1;
    Action<Pair<P1, P2>>& a;

  public:
    ApplyPairAction(const P1& p1_, Action<Pair<P1, P2>>& a_)
    : p1(p1_), a(a_) { }

    void operator()(const P2& p2) {
      a(Pair<P1, P2>(p1, p2));
    }
  };

  template <typename P1, typename P2>
  class ForwardPairAction : public Action<P1> {
  private:
    Sequence<P2>* p2s;
    Action<Pair<P1, P2>>& p;

  public:
    ForwardPairAction(Sequence<P2>* p2s_, Action<Pair<P1, P2>>& p_)
      : p2s(p2s_), p(p_) { }

    void operator()(const P1& p1) {
      ApplyPairAction<P1, P2> action(p1, p);
      p2s->Iterate(action);
    }
  };

  template <typename P1, typename P2>
  class SequenceProduct2 : public Sequence<Pair<P1, P2>> {
  private:
    Sequence<P1>* p1s;
    Sequence<P2>* p2s;

  public:
    SequenceProduct2(Sequence<P1>* p1s_, Sequence<P2>* p2s_)
      : p1s(p1s_), p2s(p2s_) { }

    void Iterate(Action<Pair<P1, P2>>& a) const {
      ForwardPairAction<P1, P2> p1a(p2s, a);
      p1s->Iterate(p1a);
    }
  };

  template <typename P1, typename P2, typename P3>
  class SequenceProduct3 : public SequenceProduct2<P1, Pair<P2, P3>> {
  private:
    SequenceProduct2<P2, P3> product2;

  public:
    SequenceProduct3(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_)
      : SequenceProduct2<P1, Pair<P2, P3>>(p1s_, &product2),
        product2(p2s_, p3s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4>
  class SequenceProduct4 : public SequenceProduct3<P1, P2, Pair<P3, P4>> {
  private:
    SequenceProduct2<P3, P4> product2;

  public:
    SequenceProduct4(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_)
      : SequenceProduct3<P1, P2, Pair<P3, P4>>(p1s_, p2s_, &product2),
        product2(p3s_, p4s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5>
  class SequenceProduct5 : public SequenceProduct4<P1, P2, P3, Pair<P4, P5>> {
  private:
    SequenceProduct2<P4, P5> product2;

  public:
    SequenceProduct5(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_)
      : SequenceProduct4<P1, P2, P3, Pair<P4, P5>>(p1s_, p2s_, p3s_, &product2),
        product2(p4s_, p5s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  class SequenceProduct6 : public SequenceProduct5<P1, P2, P3, P4, Pair<P5, P6>> {
  private:
    SequenceProduct2<P5, P6> product2;

  public:
    SequenceProduct6(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_, Sequence<P6>* p6s_)
      : SequenceProduct5<P1, P2, P3, P4, Pair<P5, P6>>(p1s_, p2s_, p3s_, p4s_, &product2),
        product2(p5s_, p6s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  class SequenceProduct7 : public SequenceProduct6<P1, P2, P3, P4, P5, Pair<P6, P7>> {
  private:
    SequenceProduct2<P6, P7> product2;

  public:
    SequenceProduct7(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_, Sequence<P6>* p6s_, Sequence<P7>* p7s_)
      : SequenceProduct6<P1, P2, P3, P4, P5, Pair<P6, P7>>(p1s_, p2s_, p3s_, p4s_, p5s_, &product2),
        product2(p6s_, p7s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  class SequenceProduct8 : public SequenceProduct7<P1, P2, P3, P4, P5, P6, Pair<P7, P8>> {
  private:
    SequenceProduct2<P7, P8> product2;

  public:
    SequenceProduct8(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_, Sequence<P6>* p6s_, Sequence<P7>* p7s_, Sequence<P8>* p8s_)
      : SequenceProduct7<P1, P2, P3, P4, P5, P6, Pair<P7, P8>>(p1s_, p2s_, p3s_, p4s_, p5s_, p6s_, &product2),
        product2(p7s_, p8s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
  class SequenceProduct9 : public SequenceProduct8<P1, P2, P3, P4, P5, P6, P7, Pair<P8, P9>> {
  private:
    SequenceProduct2<P8, P9> product2;

  public:
    SequenceProduct9(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_, Sequence<P6>* p6s_, Sequence<P7>* p7s_, Sequence<P8>* p8s_, Sequence<P9>* p9s_)
      : SequenceProduct8<P1, P2, P3, P4, P5, P6, P7, Pair<P8, P9>>(p1s_, p2s_, p3s_, p4s_, p5s_, p6s_, p7s_, &product2),
        product2(p8s_, p9s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
  class SequenceProduct10 : public SequenceProduct9<P1, P2, P3, P4, P5, P6, P7, P8, Pair<P9, P10>> {
  private:
    SequenceProduct2<P9, P10> product2;

  public:
    SequenceProduct10(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_, Sequence<P6>* p6s_, Sequence<P7>* p7s_, Sequence<P8>* p8s_, Sequence<P9>* p9s_, Sequence<P10>* p10s_)
      : SequenceProduct9<P1, P2, P3, P4, P5, P6, P7, P8, Pair<P9, P10>>(p1s_, p2s_, p3s_, p4s_, p5s_, p6s_, p7s_, p8s_, &product2),
        product2(p9s_, p10s_) { }
  };

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
  class SequenceProduct11 : public SequenceProduct10<P1, P2, P3, P4, P5, P6, P7, P8, P9, Pair<P10, P11>> {
  private:
    SequenceProduct2<P10, P11> product2;

  public:
    SequenceProduct11(Sequence<P1>* p1s_, Sequence<P2>* p2s_, Sequence<P3>* p3s_, Sequence<P4>* p4s_, Sequence<P5>* p5s_, Sequence<P6>* p6s_, Sequence<P7>* p7s_, Sequence<P8>* p8s_, Sequence<P9>* p9s_, Sequence<P10>* p10s_, Sequence<P10>* p11s_)
      : SequenceProduct10<P1, P2, P3, P4, P5, P6, P7, P8, P9, Pair<P10, P11>>(p1s_, p2s_, p3s_, p4s_, p5s_, p6s_, p7s_, p8s_, p9s_, &product2),
        product2(p10s_, p11s_) { }
  };

  template <typename P1, typename P2>
  SequenceProduct2<P1, P2>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s)
  {
    return NEWA SequenceProduct2<P1, P2>(p1s, p2s);
  }

  template <typename P1, typename P2, typename P3>
  inline SequenceProduct3<P1, P2, P3>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s)
  {
    return NEWA SequenceProduct3<P1, P2, P3>(p1s, p2s, p3s);
  }

  template <typename P1, typename P2, typename P3, typename P4>
  inline SequenceProduct4<P1, P2, P3, P4>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s)
  {
    return NEWA SequenceProduct4<P1, P2, P3, P4>(p1s, p2s, p3s, p4s);
  }
  template <typename P1, typename P2, typename P3, typename P4, typename P5>
  inline SequenceProduct5<P1, P2, P3, P4, P5>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s)
  {
    return NEWA SequenceProduct5<P1, P2, P3, P4, P5>(p1s, p2s, p3s, p4s, p5s);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
  inline SequenceProduct6<P1, P2, P3, P4, P5, P6>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s, Sequence<P6>* p6s)
  {
    return NEWA SequenceProduct6<P1, P2, P3, P4, P5, P6>(p1s, p2s, p3s, p4s, p5s, p6s);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
  inline SequenceProduct7<P1, P2, P3, P4, P5, P6, P7>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s, Sequence<P6>* p6s, Sequence<P7>* p7s)
  {
    return NEWA SequenceProduct7<P1, P2, P3, P4, P5, P6, P7>(p1s, p2s, p3s, p4s, p5s, p6s, p7s);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
  inline SequenceProduct8<P1, P2, P3, P4, P5, P6, P7, P8>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s, Sequence<P6>* p6s, Sequence<P7>* p7s, Sequence<P8>* p8s)
  {
    return NEWA SequenceProduct8<P1, P2, P3, P4, P5, P6, P7, P8>(p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
  inline SequenceProduct9<P1, P2, P3, P4, P5, P6, P7, P8, P9>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s, Sequence<P6>* p6s, Sequence<P7>* p7s, Sequence<P8>* p8s, Sequence<P9>* p9s)
  {
    return NEWA SequenceProduct9<P1, P2, P3, P4, P5, P6, P7, P8, P9>(p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
  inline SequenceProduct10<P1, P2, P3, P4, P5, P6, P7, P8, P9, P10>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s, Sequence<P6>* p6s, Sequence<P7>* p7s, Sequence<P8>* p8s, Sequence<P9>* p9s, Sequence<P10>* p10s)
  {
    return NEWA SequenceProduct10<P1, P2, P3, P4, P5, P6, P7, P8, P9, P10>(p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s, p10s);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
  inline SequenceProduct11<P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11>*
  SequenceProduct(Arena* ap, Sequence<P1>* p1s, Sequence<P2>* p2s, Sequence<P3>* p3s, Sequence<P4>* p4s, Sequence<P5>* p5s, Sequence<P6>* p6s, Sequence<P7>* p7s, Sequence<P8>* p8s, Sequence<P9>* p9s, Sequence<P10>* p10s, Sequence<P11>* p11s)
  {
    return NEWA SequenceProduct11<P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11>(p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s, p10s, p11s);
  }

  template<typename T>
  class MapSequenceBase : public Sequence<T*> {
  protected:
    Arena* ap;

  public:
    MapSequenceBase(Arena* ap_)
      : ap(ap_) { }
  };

  template<typename T, typename P1>
  class SequenceMap1 : public Sequence<T*> {
  private:
    Arena* ap;
    Sequence<P1>* s;

    class MapAction : public Action<P1> {
    private:
      Arena* ap;
      Action<T*>& a;

    public:
      MapAction(Arena* ap_, Action<T*>& a_)
        : ap(ap_), a(a_) { }

      void operator()(const P1& p) {
        a(NEWA T(p));
      }
    };

  public:
    explicit SequenceMap1(Arena* ap_, Sequence<P1>* s_)
      : ap(ap_), s(s_) { }

    void Iterate(Action<T*>& a) const {
      MapAction ma(ap, a);
      s->Iterate(ma);
    }
  };

  template<typename T, typename P1, typename P2>
  class SequenceMap2 : public Sequence<T*> {
  private:
    Arena* ap;
    Sequence<Pair<P1, P2>>* s;

    class MapAction : public Action<Pair<P1, P2>> {
    private:
      Arena* ap;
      Action<T*>& a;

    public:
      MapAction(Arena* ap_, Action<T*>& a_)
        : ap(ap_), a(a_) { }

      void operator()(const Pair<P1, P2>& p) {
        a(NEWA T(p.First(), p.Second()));
      }
    };

  public:
    explicit SequenceMap2(Arena* ap_, Sequence<Pair<P1, P2>>* s_)
      : ap(ap_), s(s_) { }

    void Iterate(Action<T*>& a) const {
      MapAction ma(ap, a);
      s->Iterate(ma);
    }
  };

  template<typename T, typename P1, typename P2, typename P3>
  class SequenceMap3 : public Sequence<T*> {
  private:
    Arena* ap;
    Sequence<Pair<P1, Pair<P2, P3>>>* s;

    class MapAction : public Action<Pair<P1, Pair<P2, P3>>> {
    private:
      Arena* ap;
      Action<T*>& a;

    public:
      MapAction(Arena* ap_, Action<T*>& a_)
        : ap(ap_), a(a_) { }

      void operator()(const Pair<P1, Pair<P2, P3>>& p) {        
        a(NEWA T(p.First(), p.Second().First(), p.Second().Second()));
      }
    };

  public:
    explicit SequenceMap3(Arena* ap_, Sequence<Pair<P1, Pair<P2, P3>>>* s_)
      : ap(ap_), s(s_) { }

    void Iterate(Action<T*>& a) const {
      MapAction ma(ap, a);
      s->Iterate(ma);
    }
  };

  template<typename T, typename P1, typename P2, typename P3, typename P4>
  class SequenceMap4 : public Sequence<T*> {
  private:
    Arena* ap;
    Sequence<Pair<P1, Pair<P2, Pair<P3, P4>>>>* s;

    class MapAction : public Action<Pair<P1, Pair<P2, Pair<P3, P4>>>> {
    private:
      Arena* ap;
      Action<T*>& a;

    public:
      MapAction(Arena* ap_, Action<T*>& a_)
        : ap(ap_), a(a_) { }

      void operator()(const Pair<P1, Pair<P2, Pair<P3, P4>>>& p) {
        a(NEWA T(p.First(), p.Second().First(), p.Second().Second().First(), p.Second().Second().Second()));
      }
    };

  public:
    explicit SequenceMap4(Arena* ap_, Sequence<Pair<P1, Pair<P2, Pair<P3, P4>>>>* s_)
      : ap(ap_), s(s_) { }

    void Iterate(Action<T*>& a) const {
      MapAction ma(ap, a);
      s->Iterate(ma);
    }
  };

  template<typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
  class SequenceMap5 : public Sequence<T*> {
  private:
    Arena* ap;
    Sequence<Pair<P1, Pair<P2, Pair<P3, Pair<P4, P5>>>>>* s;

    class MapAction : public Action<Pair<P1, Pair<P2, Pair<P3, Pair<P4, P5>>>>> {
    private:
      Arena* ap;
      Action<T*>& a;

    public:
      MapAction(Arena* ap_, Action<T*>& a_)
        : ap(ap_), a(a_) { }

      void operator()(const Pair<P1, Pair<P2, Pair<P3, Pair<P4, P5>>>>& p) {
        a(NEWA T(p.First(), p.Second().First(), p.Second().Second().First(), p.Second().Second().Second().First(), p.Second().Second().Second().Second()));
      }
    };

  public:
    explicit SequenceMap5(Arena* ap_, Sequence<Pair<P1, Pair<P2, Pair<P3, Pair<P4, P5>>>>>* s_)
      : ap(ap_), s(s_) { }

    void Iterate(Action<T*>& a) const {
      MapAction ma(ap, a);
      s->Iterate(ma);
    }
  };

  template <typename T, typename P1>
  inline SequenceMap1<T, P1>*
  SequenceMap(Arena* ap, Sequence<P1>* sequence) {
    return NEWA SequenceMap1<T, P1>(ap, sequence);
  }

  template <typename T, typename P1, typename P2>
  inline SequenceMap2<T, P1, P2>*
  SequenceMap(Arena *ap, Sequence<Pair<P1, P2>>* sequence) {
    return NEWA SequenceMap2<T, P1, P2>(ap, sequence);
  }

  template <typename T, typename P1, typename P2, typename P3>
  inline SequenceMap3<T, P1, P2, P3>*
  SequenceMap(Arena* ap, Sequence<Pair<P1, Pair<P2, P3>>>* sequence) {
    return new (ap) SequenceMap3<T, P1, P2, P3>(ap, sequence);
  }

  template <typename T, typename P1, typename P2, typename P3, typename P4>
  inline SequenceMap4<T, P1, P2, P3, P4>*
  SequenceMap(Arena* ap, Sequence<Pair<P1, Pair<P2, Pair<P3, P4>>>>* sequence) {
    return new (ap) SequenceMap4<T, P1, P2, P3, P4>(ap, sequence);
  }

  template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
  inline SequenceMap5<T, P1, P2, P3, P4, P5>*
  SequenceMap(Arena* ap, Sequence<Pair<P1, Pair<P2, Pair<P3, Pair<P4, P5>>>>>* sequence) {
    return new (ap) SequenceMap5<T, P1, P2, P3, P4, P5>(ap, sequence);
  }

  template <typename T>
  class SubsequenceAction : public Action<T> {
  private:
    uint32_t bits;
    Action<T>& action;
    unsigned index;

  public:
    SubsequenceAction(uint32_t bits_, Action<T>& action_)
      : bits(bits_), action(action_), index(0) { }

    void operator()(const T& t) {
      assert(index < 16);
      if (bits & (1 << index)) {
        action(t);
      }
      index++;
    }
  };

  template <typename T>
  class SubsetSequence : public Sequence<T> {
  private:
    const Sequence<T>* sequence;
    uint32_t bits;

  public:
    SubsetSequence(const Sequence<T>* sequence_, uint32_t bits_ = 0)
      : sequence(sequence_), bits(bits_) { }

    void SetBits(uint32_t bits) { this->bits = bits; }

    void Iterate(Action<T>& a) const {
      SubsequenceAction<T> as(bits, a);
      sequence->Iterate(as);
    }
  };

  template <typename T>
  class SubsetsSequence : public Sequence<Sequence<T>*> {
  private:
    Arena* ap;
    const Sequence<T>* sequence;
    mutable std::vector<SubsetSequence<T>*> subsequences;
    unsigned count;

  public:
    explicit SubsetsSequence(Arena* ap_, const Sequence<T>* sequence_)
      : ap(ap_), sequence(sequence_), count(sequence->Count())
    {
      assert(count <= 8); // Let's be reasonable.
    }

    void Iterate(Action<Sequence<T>*>& a) const {
      for (size_t i = subsequences.size(); i < (uint64_t) (1 << count); ++i) {
        subsequences.push_back(NEWA SubsetSequence<T>(sequence, (unsigned) i));
      }
      for (SubsetSequence<T>* subsequence : subsequences) {
        a(subsequence);
      }
    }
  };

  template <typename T>
  inline SubsetsSequence<T>*
    Subsets(Arena* ap, const Sequence<T>* sequence) { return new (ap) SubsetsSequence<T>(ap, sequence); }

}

#endif // HC_SEQUENCE_HPP
