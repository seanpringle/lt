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
#include "lt.h"

void
op_nop ()
{

}

void
op_print ()
{
  int items = depth();

  for (int i = 0; i < items; i++)
  {
    char *str = to_char(vec_get(stack(), stack()->count - items + i)[0]);
    fprintf(stream_output, "%s%s", i ? "\t": "", str);
    discard(str);
  }
  fprintf(stream_output, "\n");
  fflush(stream_output);
}

void
op_coroutine ()
{
  cor_t *cor = cor_incref(cor_alloc());
  cor->ip = pop_int();
  push(cor);
}

void
op_resume ()
{
  cor_t *cor = item(0)[0];
  ensure(is_cor(cor)) errorf("%s not a cor_t", __func__);

  if (cor->state == COR_DEAD)
  {
    push(to_bool(0));
    push(strf("cannot resume dead coroutine"));
    discard(cor);
    return;
  }

  cor->state = COR_RUNNING;

  int items = depth();

  for (int i = 1; i < items; i++)
    vec_push(cor->stack)[0] = item(i)[0];

  stack()->count -= depth();
  routines[routine_count++] = cor;
}

void
op_yield ()
{
  int items = depth();

  cor_t *src = routine();
  routine_count--;
  cor_t *dst = routine();

  for (int i = 0; i < items; i++)
    push(vec_get(src->stack, src->stack->count - items + i)[0]);

  src->stack->count -= items;
  src->state = COR_SUSPENDED;
  ivec_cell(&dst->marks, -1)[0] += items;

  discard(src);
}

void
op_call ()
{
  op_scope();

  void *ptr = pop();

  ivec_push(&routine()->calls, routine()->loops.count);
  ivec_push(&routine()->calls, routine()->marks.count);
  ivec_push(&routine()->calls, routine()->ip);

  ensure(is_int(ptr))
  {
    errorf("invalid function");
    stacktrace();
  }

  routine()->ip = get_int(ptr);
  discard(ptr);
}

void
op_call_lit ()
{
  op_find_lit();
  op_call();
}

void
op_return ()
{
  op_unscope();

  if (routine()->calls.count == 0)
  {
    routine()->state = COR_DEAD;
    op_yield();
    return;
  }

  routine()->ip = ivec_pop(&routine()->calls);

  ensure (ivec_pop(&routine()->calls) == routine()->marks.count)
    errorf("mark stack mismatch (return)");

  ensure (ivec_pop(&routine()->calls) == routine()->loops.count)
    errorf("loop stack mismatch (return)");
}

void
op_reply ()
{
  routine()->marks.count = ivec_cell(&routine()->calls, -2)[0];
  routine()->loops.count = ivec_cell(&routine()->calls, -3)[0];
  while (depth()) op_drop();
}

void
op_break ()
{
  routine()->ip = ivec_cell(&routine()->loops, -1)[0];
  routine()->marks.count = ivec_cell(&routine()->loops, -2)[0];
  while (depth()) op_drop();
}

void
op_continue ()
{
  routine()->ip = ivec_cell(&routine()->loops, -1)[0]-1;
  routine()->marks.count = ivec_cell(&routine()->loops, -2)[0];
  while (depth()) op_drop();
}

void
op_lit ()
{
  push(copy(code[routine()->ip-1].ptr));
}

void
op_scope ()
{
  vec_push(routine()->scopes)[0] = map_incref(map_alloc());
}

void
op_smudge ()
{
  scope_writing()->flags |= MAP_SMUDGED;
}

void
op_litstack ()
{
  vec_t *vec = vec_incref(vec_alloc());

  int items = depth();

  for (int i = 0; i < items; i++)
    vec_push(vec)[0] = vec_get(stack(), stack()->count - items + i)[0];

  stack()->count -= items;

  push(vec);
}

void
op_unscope ()
{
  discard(vec_pop(routine()->scopes));
}

void
op_litscope ()
{
  map_t *map = vec_pop(routine()->scopes);
  map->flags &= ~MAP_SMUDGED;
  push(map);
}

void
op_loop ()
{
  ivec_push(&routine()->loops, routine()->marks.count);
  ivec_push(&routine()->loops, code[routine()->ip-1].offset);
}

void
op_unloop ()
{
  ivec_pop(&routine()->loops);

  ensure (ivec_pop(&routine()->loops) == routine()->marks.count)
    errorf("mark stack mismatch (unloop)");
}

void
op_mark ()
{
  ivec_push(&routine()->marks, stack()->count);
}

void
op_unmark ()
{
  ivec_pop(&routine()->marks);
}

void
op_limit ()
{
  int count = code[routine()->ip-1].offset;
  int old_depth = ivec_pop(&routine()->marks);
  int req_depth = old_depth + count;
  if (count >= 0)
  {
    while (req_depth < stack()->count) op_drop();
    while (req_depth > stack()->count) push(NULL);
  }
}

void
op_string ()
{
  push(copy(super_str));
}

void
op_array ()
{
  push(copy(super_vec));
}

void
op_table ()
{
  push(copy(super_map));
}

void
op_global ()
{
  push(copy(scope_global));
}

void
op_local ()
{
  push(copy(scope_reading()));
}

void
op_self ()
{
  push(copy(self()));
}

void
op_self_push ()
{
  vec_push(routine()->selves)[0] = copy(top());
}

void
op_self_drop ()
{
  discard(vec_pop(routine()->selves));
}

void
op_shunt ()
{
  vec_push(routine()->other)[0] = pop();
}

void
op_shift ()
{
  push(vec_pop(routine()->other));
}

void
op_nil ()
{
  push(NULL);
}

void
op_true ()
{
  push(bool_true);
}

void
op_false ()
{
  push(bool_false);
}

void
op_drop ()
{
  discard(pop());
}

void
op_drop_all ()
{
  while(depth() > 0)
    op_drop();
}

void
op_test ()
{
  routine()->flags = 0;
  if (truth(top()))
    routine()->flags |= FLAG_TRUE;
  op_drop();
}

void
op_jmp ()
{
  routine()->ip = code[routine()->ip-1].offset;
}

void
op_jfalse ()
{
  if (!(routine()->flags & FLAG_TRUE))
    op_jmp();
}

void
op_jtrue ()
{
  if (routine()->flags & FLAG_TRUE)
    op_jmp();
}

void
op_for ()
{
  void *name = code[routine()->ip-1].ptr;

  int step = pop_int();
  void *iter = top();

  if (is_int(iter) || is_dbl(iter))
  {
    if (step == get_int(iter))
    {
      routine()->ip = code[routine()->ip-1].offset;
    }
    else
    {
      map_set(scope_writing(), name)[0] = to_int(step++);
      push_int(step);
    }
  }
  else
  if (is_vec(iter))
  {
    if (step == count(iter))
    {
      routine()->ip = code[routine()->ip-1].offset;
    }
    else
    {
      map_set(scope_writing(), name)[0] = copy(vec_get(iter, step++)[0]);
      push_int(step);
    }
  }
}

void
op_keys ()
{
  map_t *map = pop();
  op_mark();

  for (int chain = 0; chain < 17; chain++)
    for (node_t *node = map->chains[chain]; node; node = node->next)
      push(copy(node->key));

  op_litstack();
  ivec_pop(&routine()->marks);
}

void
op_values ()
{
  map_t *map = pop();
  op_mark();

  for (int chain = 0; chain < 17; chain++)
    for (node_t *node = map->chains[chain]; node; node = node->next)
      push(copy(node->val));

  op_litstack();
  ivec_pop(&routine()->marks);
}

void
op_assign ()
{
  void *key = pop();
  int index = code[routine()->ip-1].offset;
  void *val = index < depth() ? item(index)[0]: NULL;
  map_set(scope_writing(), key)[0] = copy(val);
  discard(key);
}

void
op_assign_lit ()
{
  int index = code[routine()->ip-1].offset;
  void *val = index < depth() ? item(index)[0]: NULL;
  map_set(scope_writing(), code[routine()->ip-1].ptr)[0] = copy(val);
}

void
op_find ()
{
  char *key = pop();
  void **ptr = map_get(scope_reading(), key);
  if (!ptr) ptr = map_get(scope_global, key);
  if (!ptr) ptr = map_get(scope_core, key);
  push(ptr ? copy(ptr[0]): NULL);
  discard(key);
}

void
op_find_lit ()
{
  char *key = code[routine()->ip-1].ptr;
  void **ptr = map_get(scope_reading(), key);
  if (!ptr) ptr = map_get(scope_global, key);
  if (!ptr) ptr = map_get(scope_core, key);

  ensure(ptr)
  {
    errorf("what? %s", key);
    stacktrace();
  }
  push(ptr ? copy(ptr[0]): NULL);
}

void
op_set ()
{
  void *val = pop();
  void *key = pop();
  void *dst = pop();
  push(val);

  if (is_vec(dst) && is_int(key))
  {
    vec_set(dst, get_int(key))[0] = copy(val);
  }
  else
  if (is_map(dst) && key)
  {
    map_set(dst, key)[0] = copy(val);
  }

  discard(key);
  discard(dst);
}

void
op_inherit ()
{
  void *dst = pop();
  void *src = pop();
  map_chain(dst, src);
  push(dst);
  discard(src);
}

void
op_get ()
{
  void *key = pop();
  void *src = pop();

  if (is_str(src) && is_str(key))
  {
    void **ptr = map_get(super_str, key);
    push(ptr ? copy(ptr[0]): NULL);
  }
  else
  if (is_vec(src) && is_int(key))
  {
    void **ptr = vec_get(src, get_int(key));
    push(ptr ? copy(ptr[0]): NULL);
  }
  else
  if (is_map(src) && key)
  {
    void **ptr = map_get(src, key);
    push(ptr ? copy(ptr[0]): NULL);
  }
  else
    push(NULL);

  discard(key);
  discard(src);
}

void
op_get_lit ()
{
  void *key = code[routine()->ip-1].ptr;
  void *src = pop();

  if (is_str(src) && is_str(key))
  {
    void **ptr = map_get(super_str, key);
    push(ptr ? copy(ptr[0]): NULL);
  }
  else
  if (is_vec(src) && is_int(key))
  {
    void **ptr = vec_get(src, get_int(key));
    push(ptr ? copy(ptr[0]): NULL);
  }
  else
  if (is_map(src) && key)
  {
    void **ptr = map_get(src, key);
    push(ptr ? copy(ptr[0]): NULL);
  }
  else
    push(NULL);

  discard(src);
}

void
op_add ()
{
  if (is_int(under()))
    push_int(pop_int()+pop_int());
  else
  if (is_dbl(under()))
    push_dbl(pop_dbl()+pop_dbl());
  else
  {
    stacktrace();
    abort();
  }
}

void
op_add_lit ()
{
  if (is_int(top()))
    ((int64_t*)top())[0] += get_int(code[routine()->ip-1].ptr);
  else
  if (is_dbl(top()))
    ((double*)top())[0] += get_dbl(code[routine()->ip-1].ptr);
  else
  {
    stacktrace();
    abort();
  }
}

void
op_neg ()
{
  if (is_int(under()))
    push_int(-pop_int());
  else
  if (is_dbl(under()))
    push_dbl(-pop_dbl());
  else
  {
    stacktrace();
    abort();
  }
}

void
op_sub ()
{
  op_neg();
  op_add();
}

void
op_mul ()
{
  if (is_int(under()))
    push_int(pop_int()*pop_int());
  else
  if (is_dbl(under()))
    push_dbl(pop_dbl()*pop_dbl());
  else
  {
    stacktrace();
    abort();
  }
}

void
op_div ()
{
  if (is_int(under()))
    { int64_t b = pop_int(), a = pop_int(); push_int(a/b); }
  else
  if (is_dbl(under()))
    { double b = pop_dbl(), a = pop_dbl(); push_dbl(a/b); }
  else
  {
    stacktrace();
    abort();
  }
}

void
op_mod ()
{
  if (is_int(under()))
    { int64_t b = pop_int(), a = pop_int(); push_int(a%b); }
  else
  {
    stacktrace();
    abort();
  }
}

void
op_eq ()
{
  void *b = pop();
  void *a = pop();
  push_flag(equal(a, b));
  discard(a);
  discard(b);
}

void
op_ne ()
{
  op_eq();
  op_not();
}

void
op_lt ()
{
  void *b = pop();
  void *a = pop();
  push_flag(less(a, b));
  discard(a);
  discard(b);
}

void
op_lt_lit ()
{
  void *a = pop();
  push_flag(less(a, code[routine()->ip-1].ptr));
  discard(a);
}

void
op_gt ()
{
  void *b = pop();
  void *a = pop();
  push_flag(!(less(a, b) || equal(a, b)));
  discard(a);
  discard(b);
}

void
op_lte ()
{
  void *b = pop();
  void *a = pop();
  push_flag(less(a, b) || equal(a, b));
  discard(a);
  discard(b);
}

void
op_gte ()
{
  void *b = pop();
  void *a = pop();
  push_flag(!less(a, b));
  discard(a);
  discard(b);
}

void
op_not ()
{
  push_flag(pop_bool() == 0);
}

void
op_concat ()
{
  void *b = pop();
  void *a = pop();
  char *bs = to_char(b);
  char *as = to_char(a);
  push(strf("%s%s", as, bs));
  discard(a);
  discard(b);
  discard(as);
  discard(bs);
}

void
op_count ()
{
  void *a = pop();
  push_int(count(a));
  discard(a);
}

void
op_match()
{
  const char *pattern = pop();
  const char *subject = pop();

  const char *error;
  int erroffset;
  int ovector[100];
  pcre_extra *extra = NULL;

  pcre *re = pcre_compile(pattern, PCRE_DOTALL|PCRE_UTF8, &error, &erroffset, 0);

  if (!re)
  {
    return;
  }

#ifdef PCRE_STUDY_JIT_COMPILE
  error = NULL;
  extra = pcre_study(re, PCRE_STUDY_JIT_COMPILE, &error);

  if (!extra && error)
  {
    pcre_free(re);
    return;
  }
#endif

  int matches = pcre_exec(re, extra, subject, strlen(subject), 0, 0, ovector, sizeof(ovector));

  if (matches < 0)
  {
    if (extra)
      pcre_free(extra);
    pcre_free(re);
    return;
  }

  if (matches == 0)
  {
    matches = sizeof(ovector)/3;
  }

  char *buffer = heap_alloc(strlen(subject)+1);

  for (int i = 0; i < matches; i++)
  {
    int offset = ovector[2*i];
    int length = ovector[2*i+1] - offset;
    memmove(buffer, subject+offset, length);
    buffer[length] = 0;
    push(substr(buffer, 0, length));
  }

  heap_free(buffer);

  if (extra)
    pcre_free(extra);
  pcre_free(re);
}

void
op_status ()
{
  map_t *status = map_incref(map_alloc());
  map_set_str(status, "heap_mem")[0] = to_int(heap_mem);
  map_set_str(status, "heap_limit")[0] = to_int(heap->pages);
  map_set_str(status, "heap_used")[0] = to_int(arena_usage(heap));
  map_set_str(status, "ints_mem")[0] = to_int(ints_mem);
  map_set_str(status, "ints_limit")[0] = to_int(ints->pages);
  map_set_str(status, "ints_used")[0] = to_int(arena_usage(ints));
  map_set_str(status, "dbls_mem")[0] = to_int(dbls_mem);
  map_set_str(status, "dbls_limit")[0] = to_int(dbls->pages);
  map_set_str(status, "dbls_used")[0] = to_int(arena_usage(dbls));
  map_set_str(status, "strs_mem")[0] = to_int(strs_mem);
  map_set_str(status, "strs_limit")[0] = to_int(strs->pages);
  map_set_str(status, "strs_used")[0] = to_int(arena_usage(strs));
  map_set_str(status, "vecs_mem")[0] = to_int(vecs_mem);
  map_set_str(status, "vecs_limit")[0] = to_int(vecs->pages);
  map_set_str(status, "vecs_used")[0] = to_int(arena_usage(vecs));
  map_set_str(status, "maps_mem")[0] = to_int(maps_mem);
  map_set_str(status, "maps_limit")[0] = to_int(maps->pages);
  map_set_str(status, "maps_used")[0] = to_int(arena_usage(maps));
  push(status);
}
