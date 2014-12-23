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

#include "Arena.hpp"
#include <algorithm>
#include <cstdlib>

namespace hexl {

const size_t Arena::chunkSize = 32 * 1024;

struct Chunk {
  size_t size;
  Chunk* next;
  char data[1];
};

Arena::Arena()
  : chunk(0),
    allocPos(0)
{
}

Arena::~Arena()
{
  Release();
}

size_t Arena::Used() const
{
  size_t size = 0;
  Chunk* chunk = this->chunk;
  if (chunk) { size += allocPos; chunk = chunk->next; }
  while (chunk) {
    size += chunk->size;
    chunk = chunk->next;
  }
  return size;
}

size_t Arena::Size() const
{
  size_t size = 0;
  Chunk* chunk = this->chunk;
  while (chunk) {
    size += chunk->size;
    chunk = chunk->next;
  }
  return size;
}

void* Arena::Malloc(size_t size)
{
  EnsureSpace(size);
  void *ptr = chunk->data + allocPos;
  allocPos += size;
  return ptr;
}

void Arena::Release()
{
  while (chunk) {
    Chunk* next = chunk->next;
    free(chunk);
    chunk = next;
  }
}

void Arena::Grow(size_t size)
{
  size = std::max(size, chunkSize);
  Chunk* next = chunk;
  chunk = (Chunk*) malloc(size);
  chunk->next = next;
  chunk->size = size;
  allocPos = 0;
}

void Arena::EnsureSpace(size_t size)
{
  if (!chunk || allocPos + size > chunk->size) {
    Grow(size);
  }
}

}
