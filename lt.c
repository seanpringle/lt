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
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <float.h>
#include <sys/stat.h>
#include <pcre.h>

#include "arena.h"
#include "op.h"
#include "str.h"
#include "vec.h"
#include "map.h"
#include "parse.h"
#include "lt.h"

arena_t *heap;
arena_t *ints;
arena_t *dbls;
arena_t *strs;
arena_t *vecs;
arena_t *maps;

int int_count;
int dbl_count;
int str_count;
int vec_count;
int map_count;

int heap_mem =   1*MB;
int ints_mem = 256*KB;
int dbls_mem = 256*KB;
int strs_mem =   1*MB;
int vecs_mem = 256*KB;
int maps_mem = 256*KB;

map_t *core;
map_t *super_str;
map_t *super_vec;
map_t *super_map;
vec_t *stacks;
vec_t *scopes;
int *calls;
int call_count;
int call_limit;
int ip;
int flags;

code_t *code;
int code_count;
int code_limit;
FILE *stream_output;

func_t funcs[] = {
  [OP_PRINT] = { .name = "print", .func = op_print },
  [OP_CALL] = { .name = "call", .func = op_call },
  [OP_CALL_LIT] = { .name = "call_lit", .func = op_call_lit },
  [OP_RETURN] = { .name = "return", .func = op_return },
  [OP_STRING] = { .name = "string", .func = op_string },
  [OP_ARRAY] = { .name = "array", .func = op_array },
  [OP_TABLE] = { .name = "table", .func = op_table },
  [OP_GLOBAL] = { .name = "global", .func = op_global },
  [OP_LOCAL] = { .name = "local", .func = op_local },
  [OP_STACK] = { .name = "stack", .func = op_stack },
  [OP_UNSTACK] = { .name = "unstack", .func = op_unstack },
  [OP_LITSTACK] = { .name = "litstack", .func = op_litstack },
  [OP_SCOPE] = { .name = "scope", .func = op_scope },
  [OP_SMUDGE] = { .name = "smudge", .func = op_smudge },
  [OP_UNSCOPE] = { .name = "unscope", .func = op_unscope },
  [OP_LITSCOPE] = { .name = "litscope", .func = op_litscope },
  [OP_TEST] = { .name = "test", .func = op_test },
  [OP_JMP] = { .name = "jmp", .func = op_jmp },
  [OP_JZ] = { .name = "jfalse", .func = op_jfalse },
  [OP_JNZ] = { .name = "jtrue", .func = op_jtrue },
  [OP_FOR] = { .name = "for", .func = op_for },
  [OP_KEYS] = { .name = "keys", .func = op_keys },
  [OP_VALUES] = { .name = "values", .func = op_values },
  [OP_NIL] = { .name = "nil", .func = op_nil },
  [OP_TRUE] = { .name = "true", .func = op_true },
  [OP_FALSE] = { .name = "false", .func = op_false },
  [OP_DEFNIL] = { .name = "defnil", .func = op_defnil },
  [OP_LIT] = { .name = "lit", .func = op_lit },
  [OP_DEFINE] = { .name = "define", .func = op_define },
  [OP_ASSIGN] = { .name = "assign", .func = op_assign },
  [OP_ASSIGN_LIT] = { .name = "assign_lit", .func = op_assign_lit },
  [OP_FIND] = { .name = "find", .func = op_find },
  [OP_FIND_LIT] = { .name = "find_lit", .func = op_find_lit },
  [OP_SET] = { .name = "set", .func = op_set },
  [OP_GET] = { .name = "get", .func = op_get },
  [OP_GET_LIT] = { .name = "get_lit", .func = op_get_lit },
  [OP_INHERIT] = { .name = "inherit", .func = op_inherit },
  [OP_DROP] = { .name = "drop", .func = op_drop },
  [OP_ADD] = { .name = "add", .func = op_add },
  [OP_ADD_LIT] = { .name = "add_lit", .func = op_add_lit },
  [OP_NEG] = { .name = "neg", .func = op_neg },
  [OP_SUB] = { .name = "sub", .func = op_sub },
  [OP_MUL] = { .name = "mul", .func = op_mul },
  [OP_DIV] = { .name = "div", .func = op_div },
  [OP_NOT] = { .name = "not", .func = op_not },
  [OP_EQ] = { .name = "eq", .func = op_eq },
  [OP_LT] = { .name = "lt", .func = op_lt },
  [OP_LT_LIT] = { .name = "lt_lit", .func = op_lt_lit },
  [OP_LTE] = { .name = "lte", .func = op_lte },
  [OP_GT] = { .name = "gt", .func = op_gt },
  [OP_GTE] = { .name = "gte", .func = op_gte },
  [OP_CONCAT] = { .name = "concat", .func = op_concat },
  [OP_COUNT] = { .name = "count", .func = op_count },
  [OP_MATCH] = { .name = "match", .func = op_match },
};

struct wrapper wrappers[] = {
  { .op = OP_PRINT,   .results = 0, .name = "print" },
  { .op = OP_INHERIT, .results = 1, .name = "inherit" },
  { .op = OP_KEYS,    .results = 1, .name = "keys" },
  { .op = OP_VALUES,  .results = 1, .name = "values" },
};

int is_bool (void *ptr) { return ptr == bool_true || ptr == bool_false; }
int is_int (void *ptr) { return arena_within(ints, ptr); }
int is_dbl (void *ptr) { return arena_within(dbls, ptr); }
int is_str (void *ptr) { return arena_within(strs, ptr); }
int is_vec (void *ptr) { return arena_within(vecs, ptr); }
int is_map (void *ptr) { return arena_within(maps, ptr); }

int
discard (void *ptr)
{
  if (is_bool(ptr)) return 1;
  if (is_int(ptr)) { int_count--; return arena_free(ints, ptr); }
  if (is_dbl(ptr)) { dbl_count--; return arena_free(dbls, ptr); }
  if (is_str(ptr)) { str_count--; return arena_free(strs, ptr); }
  if (is_vec(ptr)) { vec_decref(ptr); return 1; }
  if (is_map(ptr)) { map_decref(ptr); return 1; }
  return 1;
}

void*
copy (void *ptr)
{
  if (is_int(ptr)) return to_int(get_int(ptr));
  if (is_dbl(ptr)) return to_dbl(get_dbl(ptr));
  if (is_str(ptr)) return substr(ptr, 0, strlen(ptr));
  if (is_vec(ptr)) return vec_incref(ptr);
  if (is_map(ptr)) return map_incref(ptr);
  return ptr;
}

int
equal (void *a, void *b)
{
  if (is_bool(a) && is_bool(b)) return a == b;
  if (is_int(a) && is_int(b)) return get_int(a) == get_int(b);
  if (is_dbl(a) && is_dbl(b)) return fabs(get_dbl(a) - get_dbl(b)) <= DBL_MIN;
  if (is_str(a) && is_str(b)) return !strcmp(a, b);

  char *as = to_char(a);
  char *bs = to_char(b);
  int rc = !strcmp(as, bs);
  discard(as);
  discard(bs);
  return rc;
}

int
less (void *a, void *b)
{
  if (is_int(a) && is_int(b)) return get_int(a) < get_int(b);
  if (is_dbl(a) && is_dbl(b)) return get_dbl(a) < get_dbl(b);
  if (is_str(a) && is_str(b)) return strcmp(a, b) < 0;
  return 0;
}

int64_t
count (void *a)
{
  if (is_str(a)) return strlen(a);
  if (is_vec(a)) return ((vec_t*)a)->count;
  if (is_map(a)) return ((map_t*)a)->count;
  return 0;
}

int
truth (void *a)
{
  if (is_bool(a)) return get_bool(a);
  if (is_int(a) && get_int(a) != 0) return 1;
  if (is_dbl(a) && fabs(get_dbl(a) > DBL_MIN)) return 1;
  return count(a) != 0;
}

uint32_t
hash (void *item)
{
  if (is_str(item)) return str_djb_hash(item);
  return 0;
}

void*
to_bool (int state)
{
  return state ? bool_true: bool_false;
}

int
get_bool (void *ptr)
{
  return ptr == bool_true;
}

int64_t*
to_int (int64_t n)
{
  int64_t *ptr = arena_alloc(ints, sizeof(int64_t));
  int_count++;
  *ptr = n;
  return ptr;
}

int64_t
get_int (void *ptr)
{
  return is_int(ptr) ? *((int64_t*)ptr): (is_dbl(ptr) ? *((double*)ptr): 0);
}

double*
to_dbl (double n)
{
  double *ptr = arena_alloc(dbls, sizeof(double));
  dbl_count++;
  *ptr = n;
  return ptr;
}

double
get_dbl (void *ptr)
{
  return is_dbl(ptr) ? *((double*)ptr): (is_int(ptr) ? *((int64_t*)ptr): 0);
}

char*
to_char (void *ptr)
{
  if (is_bool(ptr)) return strf("%s", get_bool(ptr) ? "true": "false");
  if (is_int(ptr)) return strf("%ld", get_int(ptr));
  if (is_dbl(ptr)) return strf("%e", get_dbl(ptr));
  if (is_str(ptr)) return strf("%s", ptr);
  if (is_vec(ptr)) return vec_char(ptr);
  if (is_map(ptr)) return map_char(ptr);
  if (!ptr) return strf("nil");
  return strf("ptr: %llu", (uint64_t)ptr);
}

vec_t*
stack ()
{
  return vec_get(stacks, stacks->count-1)[0];
}

vec_t*
caller_stack ()
{
  return vec_get(stacks, stacks->count-2)[0];
}

map_t*
global ()
{
  return vec_get(scopes, 0)[0];
}

map_t*
scope_writing ()
{
  return vec_get(scopes, scopes->count-1)[0];
}

map_t*
scope_reading ()
{
  for (int i = scopes->count-1; i >= 0; i--)
  {
    map_t *map = vec_get(scopes, i)[0];
    if (!(map->flags & MAP_SMUDGED)) return map;
  }
  return global();
}

int
depth ()
{
  return stack()->count;
}

void
push (void *ptr)
{
  vec_push(stack())[0] = ptr;
}

void
push_bool (int state)
{
  push(to_bool(state));
}

void
push_int (int64_t n)
{
  push(to_int(n));
}

void
push_dbl (double n)
{
  push(to_dbl(n));
}

void
push_flag (int flag)
{
  if (code[ip].op == OP_TEST)
  {
    flags = 0;

    if (flag)
      flags |= FLAG_TRUE;

    ip++;
  }
  else
  {
    push_bool(flag);
  }
}

void*
pop ()
{
  return vec_pop(stack());
}

int
pop_bool ()
{
  void *ptr = pop();
  int state = truth(ptr);
  discard(ptr);
  return state;
}

int64_t
pop_int ()
{
  void *ptr = pop();
  int64_t n = is_int(ptr) ? get_int(ptr): (is_dbl(ptr) ? get_dbl(ptr): 0);
  discard(ptr);
  return n;
}

double
pop_dbl ()
{
  void *ptr = pop();
  double n = is_dbl(ptr) ? get_dbl(ptr): (is_int(ptr) ? get_int(ptr): 0);
  discard(ptr);
  return n;
}

void*
top ()
{
  return vec_get(stack(), stack()->count-1)[0];
}

void*
under ()
{
  return vec_get(stack(), stack()->count-1)[0];
}

code_t*
hindsight (int offset)
{
  return &code[code_count+offset];
}

code_t*
compile (int op)
{
  code_t *last = hindsight(-1);

  if (op == OP_FIND && last->op == OP_LIT)
  {
    last->op = OP_FIND_LIT;
    return last;
  }

  if (op == OP_GET && last->op == OP_LIT)
  {
    last->op = OP_GET_LIT;
    return last;
  }

  if (op == OP_ADD && last->op == OP_LIT)
  {
    last->op = OP_ADD_LIT;
    return last;
  }

  if (op == OP_LT && last->op == OP_LIT)
  {
    last->op = OP_LT_LIT;
    return last;
  }

  if (code_count == code_limit)
  {
    code_limit += 1024;
    code = realloc(code, sizeof(code_t) * code_limit);
    memset(&code[code_count], 0, sizeof(code_t) * (code_limit-code_count));
  }

  code_t *c = &code[code_count++];
  memset(c, 0, sizeof(code_t));
  c->op = op;
  return c;
}

void
decompile (code_t *c)
{
  char *str = to_char(c->ptr);
  fprintf(stderr, "%04ld  %04d  %-12s %10d   %s\n", c - code, flags, funcs[c->op].name, c->offset, str);
  fflush(stderr);
  discard(str);
}

void stacktrace ()
{
  decompile(&code[ip-1]);
  for (int i = call_count-1; i >= 0; i--)
    decompile(&code[calls[i]-1]);
}

void
run ()
{
  while (code[ip].op)
    funcs[code[ip++].op].func();
}

void
slurp ()
{
  char *path = pop();
  push(NULL);

  struct stat st;
  if (stat(path, &st) == 0)
  {
    FILE *file = fopen(path, "r");

    if (file)
    {
      size_t bytes = st.st_size;
      void *ptr = arena_alloc(strs, bytes + 1);

      size_t read = 0;
      for (int i = 0; i < 3; i++)
      {
        read += fread(ptr + read, 1, bytes - read, file);
        if (read == bytes) break;
      }
      ((char*)ptr)[bytes] = 0;

      if (read == bytes)
      {
        op_drop();
        push(ptr);
      }
      else
      {
        arena_free(strs, ptr);
      }
    }
    fclose(file);
  }
  discard(path);
}

int
main (int argc, char const *argv[])
{
  int _bt = 1, _bf = 0;
  bool_true  = &_bt;
  bool_false = &_bf;

  call_count = 0;
  call_limit = 32;
  calls = malloc(sizeof(int) * call_limit);

  code_count = 0;
  code_limit = 1024;
  code = malloc(sizeof(code_t) * code_limit);
  memset(code, 0, sizeof(code_t) * code_limit);

  stream_output = stdout;

  heap = malloc(heap_mem);
  arena_open(heap, heap_mem, 1024);

  ints = malloc(ints_mem);
  arena_open(ints, ints_mem, sizeof(int64_t));

  dbls = malloc(dbls_mem);
  arena_open(dbls, dbls_mem, sizeof(double));

  strs = malloc(strs_mem);
  arena_open(strs, strs_mem, 32);

  vecs = malloc(vecs_mem);
  arena_open(vecs, vecs_mem, sizeof(vec_t));

  maps = malloc(maps_mem);
  arena_open(maps, maps_mem, sizeof(map_t));

  core = map_incref(map_alloc());
  super_str = map_incref(map_alloc());
  super_vec = map_incref(map_alloc());
  super_map = map_incref(map_alloc());

  stacks = vec_alloc();
  scopes = vec_alloc();

  op_stack();
  op_scope();

  for (int i = 0; i < sizeof(wrappers) / sizeof(struct wrapper); i++)
  {
    char *name = substr(wrappers[i].name, 0, strlen(wrappers[i].name));
    map_set(core, name)[0] = to_int(code_count);
    compile(wrappers[i].op);
    compile(OP_RETURN)->offset = wrappers[i].results;
    discard(name);
  }

  ip = code_count;

  push(strf("%s", argv[1]));
  slurp();

  char *source = pop();
  int offset = 0;

  while (source[offset])
    offset += parse(&source[offset]);

  discard(source);

  for (code_t *c = &code[0]; c->op; c++)
    decompile(c);

  run();

  op_unscope();
  op_unstack();

  errorf("ints: %d, dbls: %d, strs: %d, vecs: %d, maps: %d", int_count, dbl_count, str_count, vec_count, map_count);

  return 0;
}
