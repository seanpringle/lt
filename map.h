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

typedef struct _node_t {
  struct _node_t *next, *current;
  void *key;
  void *val;
} node_t;

#define MAP_SMUDGED (1<<0)

typedef struct _map_t {
  node_t *chains[17];
  unsigned int flags;
  unsigned int count;
  int ref_count;
  struct _map_t *meta;
} map_t;

map_t* map_alloc ();
map_t* map_empty (map_t*);
void** map_get (map_t*, void*);
void** map_set (map_t*, void*);
map_t* map_incref (map_t*);
map_t* map_decref (map_t*);
void map_chain (map_t*, map_t*);
char* map_char (map_t*);
