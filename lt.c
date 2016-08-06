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
arena_t *cors;

int int_count;
int int_created;
int int_destroyed;
int dbl_count;
int dbl_created;
int dbl_destroyed;
int str_count;
int str_created;
int str_destroyed;
int vec_count;
int vec_created;
int vec_destroyed;
int map_count;
int map_created;
int map_destroyed;
int cor_count;
int cor_created;
int cor_destroyed;

int heap_mem;
int ints_mem;
int dbls_mem;
int strs_mem;
int vecs_mem;
int maps_mem;
int cors_mem;

map_t *scope_core;
map_t *scope_global;
map_t *super_str;
map_t *super_vec;
map_t *super_map;

cor_t **routines;
int routine_count;
int routine_limit;

code_t *code;
int code_count;
int code_limit;
FILE *stream_output;

func_t funcs[] = {
  [OP_NOP] = { .name = "nop", .func = op_nop },
  [OP_PRINT] = { .name = "print", .func = op_print },
  [OP_COROUTINE] = { .name = "coroutine", .func = op_coroutine },
  [OP_RESUME] = { .name = "resume", .func = op_resume },
  [OP_YIELD] = { .name = "yield", .func = op_yield },
  [OP_CALL] = { .name = "call", .func = op_call },
  [OP_CALL_LIT] = { .name = "call_lit", .func = op_call_lit },
  [OP_RETURN] = { .name = "return", .func = op_return },
  [OP_STRING] = { .name = "string", .func = op_string },
  [OP_ARRAY] = { .name = "array", .func = op_array },
  [OP_TABLE] = { .name = "table", .func = op_table },
  [OP_GLOBAL] = { .name = "global", .func = op_global },
  [OP_LOCAL] = { .name = "local", .func = op_local },
  [OP_LITSTACK] = { .name = "litstack", .func = op_litstack },
  [OP_SCOPE] = { .name = "scope", .func = op_scope },
  [OP_SMUDGE] = { .name = "smudge", .func = op_smudge },
  [OP_UNSCOPE] = { .name = "unscope", .func = op_unscope },
  [OP_LITSCOPE] = { .name = "litscope", .func = op_litscope },
  [OP_MARK] = { .name = "mark", .func = op_mark },
  [OP_UNMARK] = { .name = "unmark", .func = op_unmark },
  [OP_LIMIT] = { .name = "limit", .func = op_limit },
  [OP_LOOP] = { .name = "loop", .func = op_loop },
  [OP_UNLOOP] = { .name = "unloop", .func = op_unloop },
  [OP_REPLY] = { .name = "reply", .func = op_reply },
  [OP_BREAK] = { .name = "break", .func = op_break },
  [OP_CONTINUE] = { .name = "continue", .func = op_continue },
  [OP_TEST] = { .name = "test", .func = op_test },
  [OP_JMP] = { .name = "jmp", .func = op_jmp },
  [OP_JFALSE] = { .name = "jfalse", .func = op_jfalse },
  [OP_JTRUE] = { .name = "jtrue", .func = op_jtrue },
  [OP_FOR] = { .name = "for", .func = op_for },
  [OP_KEYS] = { .name = "keys", .func = op_keys },
  [OP_VALUES] = { .name = "values", .func = op_values },
  [OP_NIL] = { .name = "nil", .func = op_nil },
  [OP_SELF] = { .name = "self", .func = op_self },
  [OP_SELF_PUSH] = { .name = "self_push", .func = op_self_push },
  [OP_SELF_DROP] = { .name = "self_drop", .func = op_self_drop },
  [OP_SHUNT] = { .name = "shunt", .func = op_shunt },
  [OP_SHIFT] = { .name = "shift", .func = op_shift },
  [OP_TRUE] = { .name = "true", .func = op_true },
  [OP_FALSE] = { .name = "false", .func = op_false },
  [OP_LIT] = { .name = "lit", .func = op_lit },
  [OP_ASSIGN] = { .name = "assign", .func = op_assign },
  [OP_ASSIGN_LIT] = { .name = "assign_lit", .func = op_assign_lit },
  [OP_FIND] = { .name = "find", .func = op_find },
  [OP_FIND_LIT] = { .name = "find_lit", .func = op_find_lit },
  [OP_SET] = { .name = "set", .func = op_set },
  [OP_GET] = { .name = "get", .func = op_get },
  [OP_GET_LIT] = { .name = "get_lit", .func = op_get_lit },
  [OP_INHERIT] = { .name = "inherit", .func = op_inherit },
  [OP_DROP] = { .name = "drop", .func = op_drop },
  [OP_DROP_ALL] = { .name = "drop_all", .func = op_drop_all },
  [OP_ADD] = { .name = "add", .func = op_add },
  [OP_ADD_LIT] = { .name = "add_lit", .func = op_add_lit },
  [OP_NEG] = { .name = "neg", .func = op_neg },
  [OP_SUB] = { .name = "sub", .func = op_sub },
  [OP_MUL] = { .name = "mul", .func = op_mul },
  [OP_DIV] = { .name = "div", .func = op_div },
  [OP_MOD] = { .name = "mod", .func = op_mod },
  [OP_NOT] = { .name = "not", .func = op_not },
  [OP_EQ] = { .name = "eq", .func = op_eq },
  [OP_NE] = { .name = "ne", .func = op_ne },
  [OP_LT] = { .name = "lt", .func = op_lt },
  [OP_LT_LIT] = { .name = "lt_lit", .func = op_lt_lit },
  [OP_LTE] = { .name = "lte", .func = op_lte },
  [OP_GT] = { .name = "gt", .func = op_gt },
  [OP_GTE] = { .name = "gte", .func = op_gte },
  [OP_CONCAT] = { .name = "concat", .func = op_concat },
  [OP_COUNT] = { .name = "count", .func = op_count },
  [OP_MATCH] = { .name = "match", .func = op_match },
  [OP_STATUS] = { .name = "status", .func = op_status },
};

struct wrapper wrappers[] = {
  { .library = &scope_core, .op = OP_STATUS,  .results = 1, .name = "status" },
  { .library = &scope_core, .op = OP_PRINT,   .results = 0, .name = "print" },
  { .library = &scope_core, .op = OP_INHERIT, .results = 1, .name = "inherit" },
  { .library = &scope_core, .op = OP_KEYS,    .results = 1, .name = "keys" },
  { .library = &scope_core, .op = OP_VALUES,  .results = 1, .name = "values" },
};

void*
heap_alloc (unsigned int bytes)
{
  void *ptr = arena_alloc(heap, bytes);
  ensure(ptr)
  {
    errorf("arena_alloc heap %u", bytes);
    stacktrace();
  }
  return ptr;
}

void*
heap_realloc (void *old, unsigned int bytes)
{
  void *ptr = arena_realloc(heap, old, bytes);
  ensure(ptr)
  {
    errorf("arena_realloc heap %u", bytes);
    stacktrace();
  }
  return ptr;
}

void
heap_free (void *ptr)
{
  int rc = arena_free(heap, ptr);
  ensure (rc == 0)
  {
    errorf("arena_free heap %d", rc);
    stacktrace();
  }
}

void 
ivec_init (ivec_t *ivec)
{
  memset(ivec, 0, sizeof(ivec_t));
  ivec->count = 0;
  ivec->limit = 8;
  ivec->items = malloc(sizeof(int64_t) * ivec->limit);
  
  ensure(ivec->items)
    errorf("%s malloc %ld", __func__, sizeof(int64_t) * ivec->limit);
}

void 
ivec_push (ivec_t *ivec, int64_t item)
{
  if (ivec->count == ivec->limit)
  {
    ivec->limit += 8;
    ivec->items = realloc(ivec->items, sizeof(int64_t) * ivec->limit);

    ensure(ivec->items)
      errorf("%s realloc %ld", __func__, sizeof(int64_t) * ivec->limit);
  }
  ivec->items[ivec->count++] = item;
}

int64_t*
ivec_cell (ivec_t *ivec, int index)
{
//  ensure ((index >= 0 && index < ivec->count) || (index < 0 && index*-1 < ivec->count))
//    errorf("%s out of bounds %d", __func__, index);
  return index < 0 ? &ivec->items[ivec->count+index]: &ivec->items[index];
}

int64_t 
ivec_pop (ivec_t *ivec)
{
  return ivec->items[--ivec->count];
}

void 
ivec_empty (ivec_t *ivec)
{
  free(ivec->items);
  memset(ivec, 0, sizeof(ivec_t));
}

cor_t*
routine ()
{
  return routines[routine_count-1];
}

cor_t*
cor_alloc ()
{
  cor_t *cor = arena_alloc(cors, sizeof(cor_t));
  memset(cor, 0, sizeof(cor_t));

  cor->stack = vec_incref(vec_alloc());
  cor->other = vec_incref(vec_alloc());
  cor->selves = vec_incref(vec_alloc());
  cor->scopes = vec_incref(vec_alloc());
  ivec_init(&cor->calls);
  ivec_init(&cor->loops);
  ivec_init(&cor->marks);
  cor->ref_count = 0;
  cor->flags = 0;
  cor->ip = 0;
  cor->state = COR_SUSPENDED;
  ivec_push(&cor->marks, 0);
  return cor;
}

cor_t*
cor_incref (cor_t *cor)
{
  cor->ref_count++;
  return cor;
}

cor_t*
cor_decref (cor_t *cor)
{
  if (--cor->ref_count == 0)
  {
    vec_decref(cor->stack);
    vec_decref(cor->other);
    vec_decref(cor->selves);
    vec_decref(cor->scopes);
    ivec_empty(&cor->calls);
    ivec_empty(&cor->loops);
    ivec_empty(&cor->marks);
    memset(cor, 0, sizeof(cor_t));
    cor = NULL;
  }
  return cor;
}

int is_bool (void *ptr) { return ptr == bool_true || ptr == bool_false; }
int is_int (void *ptr) { return arena_within(ints, ptr); }
int is_dbl (void *ptr) { return arena_within(dbls, ptr); }
int is_str (void *ptr) { return arena_within(strs, ptr); }
int is_vec (void *ptr) { return arena_within(vecs, ptr); }
int is_map (void *ptr) { return arena_within(maps, ptr); }
int is_cor (void *ptr) { return arena_within(cors, ptr); }

int
discard (void *ptr)
{
  if (is_bool(ptr)) return 1;
  if (is_int(ptr)) { int_count--; int_destroyed++; return arena_free(ints, ptr); }
  if (is_dbl(ptr)) { dbl_count--; dbl_destroyed++; return arena_free(dbls, ptr); }
  if (is_str(ptr)) { str_count--; str_destroyed++; return arena_free(strs, ptr); }
  if (is_vec(ptr)) { vec_decref(ptr); return 1; }
  if (is_map(ptr)) { map_decref(ptr); return 1; }
  if (is_cor(ptr)) { cor_decref(ptr); return 1; }
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
  if (is_cor(ptr)) return cor_incref(ptr);
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
  int_created++;
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
  dbl_created++;
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
  if (is_cor(ptr)) return strf("cor()");
  if (!ptr) return strf("nil");
  return strf("ptr: %llu", (uint64_t)ptr);
}

vec_t*
stack ()
{
  return routine()->stack;
}

map_t*
scope_writing ()
{
  return routine()->scopes->count ? vec_get(routine()->scopes, routine()->scopes->count-1)[0]: scope_global;
}

map_t*
scope_reading ()
{
  for (int i = routine()->scopes->count-1; i >= 0; i--)
  {
    map_t *map = vec_get(routine()->scopes, i)[0];
    if (!(map->flags & MAP_SMUDGED)) return map;
  }
  return scope_global;
}

void*
self ()
{
  return routine()->selves->count ? vec_get(routine()->selves, routine()->selves->count-1)[0]: NULL;
}

int
depth ()
{
  return stack()->count - ivec_cell(&routine()->marks, -1)[0];
}

void**
item (int i)
{
  return vec_get(stack(), stack()->count - depth() + i);
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
  if (code[routine()->ip].op == OP_TEST)
  {
    routine()->flags = 0;

    if (flag)
      routine()->flags |= FLAG_TRUE;

    routine()->ip++;
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
  int64_t n = get_int(ptr);
  discard(ptr);
  return n;
}

double
pop_dbl ()
{
  void *ptr = pop();
  double n = get_dbl(ptr);
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
  return vec_get(stack(), stack()->count-2)[0];
}

code_t*
hindsight (int offset)
{
  return &code[code_count+offset] >= code ? &code[code_count+offset]: NULL;
}

code_t*
compile (int op)
{
  code_t *b = hindsight(-2);
  code_t *a = hindsight(-1);

  // frames

  if (a && b && op == OP_LIMIT && a->op == OP_LIT && b->op == OP_MARK)
  {
    memmove(b, a, sizeof(code_t));
    code_count--;
    return b;
  }

  if (a && op == OP_FIND && a->op == OP_LIT)
  {
    a->op = OP_FIND_LIT;
    return a;
  }

  if (a && op == OP_GET && a->op == OP_LIT)
  {
    a->op = OP_GET_LIT;
    return a;
  }

  if (a && op == OP_CALL && a->op == OP_FIND_LIT)
  {
    a->op = OP_CALL_LIT;
    return a;
  }

//  if (a && op == OP_CALL && a->op == OP_LIT)
//  {
//    a->op = OP_CALL_LIT;
//    return a;
//  }

  // math

  if (a && op == OP_ADD && a->op == OP_LIT)
  {
    a->op = OP_ADD_LIT;
    return a;
  }

  if (a && op == OP_LT && a->op == OP_LIT)
  {
    a->op = OP_LT_LIT;
    return a;
  }

  if (code_count == code_limit)
  {
    code_limit += 1024;
    code = heap_realloc(code, sizeof(code_t) * code_limit);
    memset(&code[code_count], 0, sizeof(code_t) * (code_limit-code_count));
  }

  code_t *c = &code[code_count++];
  memset(c, 0, sizeof(code_t));
  c->op = op;

  code[code_count].op = 0;

  return c;
}

void
decompile (code_t *c)
{
  char *str = to_char(c->ptr);
  fprintf(stderr, "%04ld  %04d  %-10s %4d   %s\n", c - code, routine()->flags, funcs[c->op].name, c->offset, str);
  fflush(stderr);
  discard(str);
}

void stacktrace ()
{
  decompile(&code[routine()->ip-1]);
  for (int i = routine()->calls.count-1; i >= 0; i--)
    decompile(&code[ivec_cell(&routine()->calls, i)[0]-1]);
}

void
run ()
{
  while (code[routine()->ip].op)
  {
//    for (int i = 0; i < routine()->mark_count; i++) fprintf(stderr, "  ");
//    decompile(&code[routine()->ip]);

    int op = code[routine()->ip++].op;
    if (op != OP_NOP) funcs[op].func();

//    for (int i = 0; i < routine()->mark_count; i++) fprintf(stderr, "  ");
//    char *s = to_char(stack()); errorf("%s", s); discard(s);
//    for (int i = 0; i < routine()->mark_count; i++) fprintf(stderr, "  ");
//    for (int i = 0; i < routine()->mark_count; i++) fprintf(stderr, "%d ", routine()->marks[i]);
//    fprintf(stderr, "\n\n");
  }
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
  char *script = NULL;
  heap_mem = 8*MB;

  for (int argi = 0; argi < argc; argi++)
  {
    if ((!strcmp(argv[argi], "-m") || !strcmp(argv[argi], "--memory")) && argi+1 < argc)
    {
      heap_mem = strtoll(argv[++argi], NULL, 0) * MB;
      continue;
    }

    script = (char*)argv[argi];
  }

  ensure(script)
    errorf("expected script");

  if (heap_mem < 1*MB)
  {
    errorf("setting heap_mem = 1MB (minimum)");
    heap_mem = 1*MB;
  }

  ints_mem = heap_mem * 0.05;
  dbls_mem = heap_mem * 0.05;
  strs_mem = heap_mem * 0.25;
  vecs_mem = heap_mem * 0.01;
  maps_mem = heap_mem * 0.01;
  cors_mem = heap_mem * 0.01;

  heap = malloc(heap_mem);
  ensure(heap) errorf("malloc heap %u", heap_mem);
  arena_open(heap, heap_mem, 1024);

  ints = heap_alloc(ints_mem);
  arena_open(ints, ints_mem, sizeof(int64_t));

  dbls = heap_alloc(dbls_mem);
  arena_open(dbls, dbls_mem, sizeof(double));

  strs = heap_alloc(strs_mem);
  arena_open(strs, strs_mem, 32);

  vecs = heap_alloc(vecs_mem);
  arena_open(vecs, vecs_mem, sizeof(vec_t));

  maps = heap_alloc(maps_mem);
  arena_open(maps, maps_mem, sizeof(map_t));

  cors = heap_alloc(cors_mem);
  arena_open(cors, cors_mem, sizeof(cor_t));

  int _bt = 1, _bf = 0;
  bool_true  = &_bt;
  bool_false = &_bf;

  code_count = 0;
  code_limit = 1024;
  code = heap_alloc(sizeof(code_t) * code_limit);
  memset(code, 0, sizeof(code_t) * code_limit);

  stream_output = stdout;

  scope_core = map_incref(map_alloc());
  scope_global = map_incref(map_alloc());
  super_str = map_incref(map_alloc());
  super_vec = map_incref(map_alloc());
  super_map = map_incref(map_alloc());

  routine_count = 0;
  routines = heap_alloc(sizeof(cor_t*) * 32);

  routines[routine_count++] = cor_incref(cor_alloc());

  op_mark();
  //op_scope();

  for (int i = 0; i < sizeof(wrappers) / sizeof(struct wrapper); i++)
  {
    char *name = substr(wrappers[i].name, 0, strlen(wrappers[i].name));
    map_set(wrappers[i].library[0], name)[0] = to_int(code_count);
    compile(wrappers[i].op);
    compile(OP_RETURN);
    discard(name);
  }

  routine()->ip = code_count;

  push(strf("%s", argv[1]));
  slurp();

  ensure(top())
    errorf("failed to read %s", script);

  source(top());
  op_drop();

  for (code_t *c = &code[0]; c->op; c++)
    decompile(c);

  run();

  errorf("COUNT    ints: %3d,  dbls: %3d,  strs: %3d,  vecs: %3d,  maps: %3d  cors: %3d", int_count, dbl_count, str_count, vec_count, map_count, cor_count);
  errorf("CREATE   ints: %3d,  dbls: %3d,  strs: %3d,  vecs: %3d,  maps: %3d  cors: %3d", int_created, dbl_created, str_created, vec_created, map_created, cor_created);
  errorf("DESTROY  ints: %3d,  dbls: %3d,  strs: %3d,  vecs: %3d,  maps: %3d  cors: %3d", int_destroyed, dbl_destroyed, str_destroyed, vec_destroyed, map_destroyed, cor_destroyed);
  errorf("               %3d,        %3d,        %3d,        %3d,        %3d        %3d", int_count-(int_created-int_destroyed), dbl_count-(dbl_created-dbl_destroyed), str_count-(str_created-str_destroyed), vec_count-(vec_created-vec_destroyed), map_count-(map_created-map_destroyed), cor_count-(cor_created-cor_destroyed));

  return 0;
}

