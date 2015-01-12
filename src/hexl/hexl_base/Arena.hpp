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

#ifndef HC_ARENA_HPP
#define HC_ARENA_HPP

#include <cstddef>

namespace hexl {

struct Chunk;

class Arena {
public:
  Arena();
  ~Arena();
  size_t Used() const;
  size_t Size() const;
  void* Malloc(size_t size);
  void Release();

private:
  Arena(const Arena&);
  Arena& operator=(const Arena&);
  static const size_t chunkSize, chunkReserved;
  Chunk* chunk;
  size_t allocPos;

  void Grow(size_t size);
  void EnsureSpace(size_t size);
};

#define ARENAMEM                                                            \
public:                                                                     \
  void* operator new(size_t size, Arena* ap) { return ap->Malloc(size); }   \
  void* operator new[](size_t size, Arena* ap) { return ap->Malloc(size); } \
  void* operator new(std::size_t, void * p) throw() { return p ; }          \
  void* operator new[](std::size_t, void * p) throw() { return p ; }        \
  void operator delete(void *p) { /* Do nothing. */ }                       \
  void operator delete[](void *p) { /* Do nothing. */ }                     \
  void operator delete(void *p, Arena* ap) { /* Do nothing. */ }            \
private:                                                                    \
  void* operator new(size_t size);                                          \

#define ARENAORHEAPMEM                                                      \
public:                                                                     \
  void* operator new(size_t size, Arena* ap) { return ap->Malloc(size); }   \
  void* operator new[](size_t size, Arena* ap) { return ap->Malloc(size); } \
  void* operator new(std::size_t, void * p) throw() { return p ; }          \
  void* operator new[](std::size_t, void * p) throw() { return p ; }        \
  void operator delete(void *p) { /* Do nothing. */ }                       \
  void operator delete[](void *p) { /* Do nothing. */ }                     \
  void operator delete(void *p, Arena* ap) { /* Do nothing. */ }            \

#define NEWA new (ap)

}

#endif // HC_ARENA_HPP
