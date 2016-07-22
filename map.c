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

map_t*
map_alloc ()
{
  if (!nodes)
  {
    int node_mem = maps->pages * 17 * sizeof(node_t);
    nodes = heap_alloc(node_mem);
    arena_open(nodes, node_mem, sizeof(node_t));
  }

  map_count++;
  map_created++;
  map_t *map = arena_alloc(maps, sizeof(map_t));

  ensure(map)
  {
    stacktrace();
    errorf("arena_alloc maps");
  }
  memset(map, 0, sizeof(map_t));

  map->chains = heap_alloc(sizeof(node_t*) * 17);
  memset(map->chains, 0, sizeof(node_t*) * 17);

  return map;
}

static void
ensure_map (map_t *map, const char *func)
{
  ensure(is_map(map)) errorf("%s not a map_t", func);
}

map_t*
map_empty (map_t *map)
{
  ensure_map(map, __func__);

  for (int i = 0; i < 17; i++)
  {
    while (map->chains[i])
    {
      node_t *node = map->chains[i];
      discard(node->key);
      discard(node->val);
      map->chains[i] = node->next;
      arena_free(nodes, node);
    }
  }
  if (map->meta)
    map_decref(map->meta);
  heap_free(map->chains);
  memset(map, 0, sizeof(map_t));
  return map;
}

void**
map_get (map_t *map, void *key)
{
  ensure_map(map, __func__);

  uint32_t chain = hash(key) % 17;
  node_t *node = map->chains[chain];
  while (node && !equal(node->key, key))
    node = node->next;
  if (!node && map->meta)
    return map_get(map->meta, key);
  if (!node && map != super_map && super_map)
    return map_get(super_map, key);
  return node ? &node->val: NULL;
}

void**
map_set (map_t *map, void *key)
{
  ensure_map(map, __func__);

  uint32_t chain = hash(key) % 17;
  node_t *node = map->chains[chain];
  while (node && !equal(node->key, key))
    node = node->next;

  if (!node)
  {
    node = arena_alloc(nodes, sizeof(node_t));
    node->next = map->chains[chain];
    map->chains[chain] = node;
    node->key = copy(key);
    map->count++;
  }
  else
  {
    discard(node->val);
  }
  node->val = NULL;
  return &node->val;
}

void**
map_set_str (map_t *map, char *str)
{
  ensure_map(map, __func__);

  void *key = substr(str, 0, strlen(str));
  void **ptr = map_set(map, key);
  discard(key);
  return ptr;
}

map_t*
map_incref (map_t *map)
{
  ensure_map(map, __func__);

  map->ref_count++;
  return map;
}

map_t*
map_decref (map_t *map)
{
  ensure_map(map, __func__);

  if (--map->ref_count == 0)
  {
    map_empty(map);
    arena_free(maps, map);
    map_count--;
    map_destroyed++;
    map = NULL;
  }
  return map;
}

void
map_chain (map_t *map, map_t *meta)
{
  ensure_map(map, __func__);

  map_incref(meta);
  if (map->meta)
    map_decref(map->meta);
  map->meta = meta;
}

char*
map_char (map_t *map)
{
  ensure_map(map, __func__);

  push(strf("{"));

  int i = 0;
  for (int chain = 0; chain < 17; chain++)
  {
    for (node_t *node = map->chains[chain]; node; node = node->next)
    {
      push(to_char(node->key));
      op_concat();
      push(strf(": "));
      op_concat();

      if (is_vec(node->val))
        push(strf("vec[]"));
      else
      if (is_map(node->val))
        push(strf("map[]"));
      else
        push(to_char(node->val));

      op_concat();
      if (i < map->count-1)
      {
        push(strf(", "));
        op_concat();
      }
      i++;
    }
  }

  push(strf("}"));
  op_concat();

  return pop();
}
