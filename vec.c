/*
Copyright (c) 2016 Sean Pringle sean.pringle@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "arena.h"
#include "op.h"
#include "str.h"
#include "vec.h"
#include "map.h"
#include "lt.h"

#define VEC_STEP 32

vec_t*
vec_alloc ()
{
  vec_count++;
  vec_created++;
  vec_t *vec = arena_alloc(vecs, sizeof(vec_t));

  ensure(vec)
  {
    stacktrace();
    errorf("arena_alloc vecs");
  }
  vec->items = heap_alloc(sizeof(void*) * VEC_STEP);
  vec->count = 0;
  return vec;
}

void**
vec_ins (vec_t *vec, int index)
{
  if (index > vec->count) index = vec->count;
  if (index < 0) index = 0;

  vec->count++;

  if (vec->count % VEC_STEP == 0)
    vec->items = heap_realloc(vec->items, sizeof(void*) * (vec->count + VEC_STEP));

  memmove(&vec->items[index+1], &vec->items[index], (vec->count - index - 1) * sizeof(void*));
  vec->items[index] = NULL;
  return &vec->items[index];
}

void**
vec_set (vec_t *vec, int index)
{
  if (index > vec->count) index = vec->count;
  if (index < 0) index = 0;

  if (index == vec->count) return vec_ins(vec, index);

  discard(vec->items[index]);
  vec->items[index] = NULL;

  return &vec->items[index];
}

void**
vec_push (vec_t *vec)
{
  return vec_ins(vec, vec->count);
}

void*
vec_del (vec_t *vec, int index)
{
  if (index >= vec->count || index < 0) return NULL;

  void *ptr = vec->items[index];
  memmove(&vec->items[index], &vec->items[index+1], (vec->count - index - 1) * sizeof(void*));
  vec->count--;
  return ptr;
}

void*
vec_pop (vec_t *vec)
{
  if (!vec->count) return NULL;
  return vec_del(vec, vec->count-1);
}

void**
vec_get (vec_t *vec, int index)
{
  if (index >= vec->count || index < 0) return NULL;

  return &vec->items[index];
}

vec_t*
vec_empty (vec_t *vec)
{
  for (int i = 0; i < vec->count; i++)
    discard(vec->items[i]);
  heap_free(vec->items);
  memset(vec, 0, sizeof(vec_t));
  return vec;
}

vec_t*
vec_incref (vec_t *vec)
{
  vec->ref_count++;
  return vec;
}

vec_t*
vec_decref (vec_t *vec)
{
  if (--vec->ref_count == 0)
  {
    vec_empty(vec);
    arena_free(vecs, vec);
    vec_count--;
    vec_destroyed++;
    vec = NULL;
  }
  return vec;
}

char*
vec_char (vec_t *vec)
{
  int count = vec->count;

  push(strf("["));

  for (int i = 0; i < count; i++)
  {
    if (is_vec(vec_get(vec, i)[0]))
      push(strf("vec[]"));
    else
    if (is_map(vec_get(vec, i)[0]))
      push(strf("map[]"));
    else
      push(to_char(vec_get(vec, i)[0]));

    op_concat();
    if (i < count-1)
    {
      push(strf(", "));
      op_concat();
    }
  }

  push(strf("]"));
  op_concat();

  return pop();
}
