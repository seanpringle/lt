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

#include "arena.h"
#include "op.h"
#include "str.h"
#include "vec.h"
#include "map.h"
#include "lt.h"
#include "parse.h"

int
isname (int c)
{
  return isalpha(c) || isdigit(c) || c == '_';
}

int
islf (int c)
{
  return c == '\n';
}

int
skip_gap (char *source)
{
  int offset = 0;

  while (source[offset])
  {
    if (isspace(source[offset]))
    {
      offset += str_skip(&source[offset], isspace);
      continue;
    }
    if (source[offset] == '-' && source[offset+1] == '-')
    {
      while (source[offset] == '-' && source[offset+1] == '-')
      {
        offset += str_scan(&source[offset], islf);
        offset += str_skip(&source[offset], islf);
      }
      continue;
    }
    break;
  }
  return offset;
}

expr_t*
expr_alloc ()
{
  expr_t *expr = heap_alloc(sizeof(expr_t));
  memset(expr, 0, sizeof(expr_t));
  return expr;
}

void
expr_free (expr_t *expr)
{
  heap_free(expr);
}

int
expression (char *source)
{
  int length = 0;
  int offset = skip_gap(source);

  expr_t *expr = expr_alloc();
  expr->source = &source[offset];

  if (isalpha(source[offset]))
  {
    expr->type = EXPR_VAR;
    length = str_skip(&source[offset], isname);
    expr->item = substr(&source[offset], 0, length);
    offset += length;
  }
  else
  if (source[offset] == '"')
  {
    expr->type = EXPR_LIT;
    char *end = NULL;
    expr->item = str_unquote(&source[offset], &end);
    offset += end - &source[offset];
  }
  else
  if (isdigit(source[offset]))
  {
    expr->type = EXPR_LIT;
    char *end = NULL;
    expr->item = to_int(strtoll(&source[offset], &end, 0));
    offset += end - &source[offset];
  }
  else
  {
    ensure(0)
      errorf("what: %s", &source[offset]);
  }

  while (source[offset])
  {
    offset += skip_gap(&source[offset]);

    if (source[offset] == '(')
    {
      expr->call = 1;

      int count = depth();
      offset += parse(&source[offset], -1);
      expr->args = depth() - count;
    }

    break;
  }

  push(expr);
  return offset;
}

int
parse (char *source, int results)
{
  int offset = skip_gap(source);

  code_t *marker = compile(OP_MARK);

  vec_t *keys = vec_alloc();
  vec_t *vals = vec_alloc();
  vec_t *exprs = keys;

  int parens = source[offset] == '(';
  if (parens) offset++;

  while (source[offset])
  {
    int length;

    if ((length = skip_gap(&source[offset])) > 0)
    {
      offset += length;
      continue;
    }

    if (parens && source[offset] == ')')
    {
      offset++;
      break;
    }

    offset += expression(&source[offset]);
    vec_push(exprs)[0] = pop();

    offset += skip_gap(&source[offset]);

    if (exprs == keys && source[offset] == '=')
    {
      offset++;
      exprs = vals;
      continue;
    }

    if (source[offset] == ',')
    {
      offset++;
      continue;
    }

    if (!parens)
      break;
  }

  if (!vals->count)
  {
    exprs = keys;
    keys = vals;
    vals = exprs;
  }

  for (int i = 0; i < vals->count; i++)
  {
    expr_t *val = vec_get(vals, i)[0];

    if (val->type == EXPR_LIT)
    {
      compile(OP_LIT)->ptr = val->item;
    }
    else
    if (val->type == EXPR_VAR)
    {
      compile(OP_FIND_LIT)->ptr = val->item;
    }

    if (val->call) compile(OP_CALL);
    expr_free(val);
  }

  for (int i = 0; i < keys->count; i++)
  {
    expr_t *key = vec_get(keys, i)[0];

    ensure(key->type == EXPR_VAR)
      errorf("attempt to assign non-variable: %s", key->source);

    code_t *code = compile(OP_ASSIGN_LIT);
    code->ptr    = key->item;
    code->offset = i;
  }

  vals->count = 0;
  keys->count = 0;

  discard(keys);
  discard(vals);

  if (results == -1 && hindsight(-1)->op == OP_FIND_LIT && hindsight(-2) == marker)
  {
    memmove(marker, hindsight(-1), sizeof(code_t));
    code_count--;
  }
  else
  {
    compile(OP_LIMIT)->offset = results;
  }

  return offset + skip_gap(&source[offset]);
}

void
source (char *s)
{
  int offset = 0;

  while (s[offset])
    offset += parse(&s[offset], 0);
}
