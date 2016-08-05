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
  expr_t *expr = malloc(sizeof(expr_t));
  memset(expr, 0, sizeof(expr_t));
  return expr;
}

void
expr_free (expr_t *expr)
{
  free(expr);
}

typedef struct {
  char *name;
  int opcode;
} keyword_t;

keyword_t keywords[] = {
  { .name = "global", .opcode = OP_GLOBAL },
  { .name = "local", .opcode = OP_LOCAL },
};

int
parse_item (char *source)
{
  int length = 0;
  int offset = skip_gap(source);

  expr_t *expr = expr_alloc();
  expr->source = &source[offset];

  if (isalpha(source[offset]))
  {
    expr->type = EXPR_VARIABLE;
    length = str_skip(&source[offset], isname);

    int have_keyword = 0;

    for (int i = 0; i < sizeof(keywords) / sizeof(keyword_t) && !have_keyword; i++)
    {
      keyword_t *keyword = &keywords[i];
      int name_len = strlen(keyword->name);

      if (length == name_len && !strncmp(&source[offset], keyword->name, name_len))
      {
        expr->type = EXPR_OPCODE;
        expr->opcode = OP_GLOBAL;
        have_keyword = 1;
      }
    }

    if (!have_keyword)
      expr->item = substr(&source[offset], 0, length);

    offset += length;
  }
  else
  if (source[offset] == '"')
  {
    expr->type = EXPR_LITERAL;
    char *end = NULL;
    expr->item = str_unquote(&source[offset], &end);
    offset += end - &source[offset];
  }
  else
  if (isdigit(source[offset]))
  {
    expr->type = EXPR_LITERAL;
    char *end = NULL;
    expr->item = to_int(strtoll(&source[offset], &end, 0));
    offset += end - &source[offset];
  }
  else
  {
    ensure(0)
      errorf("what: %s", &source[offset]);
  }

  offset += skip_gap(&source[offset]);

  if (source[offset] == '(')
  {
    offset++;
    expr->call = 1;

    offset += parse(&source[offset], -1, 1);
    expr->args = pop();

    offset += skip_gap(&source[offset]);

    ensure(source[offset] == ')')
      errorf("expected closing paren: %s", source);

    offset++;
  }

  push(expr);
  return offset;
}

typedef struct {
  char *name;
  int precedence;
  int opcode;
} operator_t;

operator_t operators[] = {
  { .name = "+", .precedence = 1, .opcode = OP_ADD },
  { .name = "-", .precedence = 1, .opcode = OP_SUB },
  { .name = "*", .precedence = 2, .opcode = OP_MUL },
  { .name = "/", .precedence = 2, .opcode = OP_DIV },
};

int
parse (char *source, int results, int level)
{
  int length = 0;
  int offset = skip_gap(source);

  expr_t *expr = expr_alloc();
  expr->type = EXPR_MULTI;
  expr->results = results;

  while (source[offset])
  {
    if ((length = skip_gap(&source[offset]))> 0)
    {
      offset += length;
      continue;
    }

    operator_t *operations[32];
    expr_t *arguments[32];
    int operation = 0;
    int argument = 0;

    char *start = &source[offset];

    while (source[offset])
    {
      if ((length = skip_gap(&source[offset])) > 0)
      {
        offset += length;
        continue;
      }

      offset += parse_item(&source[offset]);
      arguments[argument++] = pop();

      int have_operator = 0;
      for (int i = 0; i < sizeof(operators) / sizeof(operator_t); i++)
      {
        operator_t *compare = &operators[i];

        if (!strncmp(compare->name, &source[offset], strlen(compare->name)))
        {
          have_operator = 1;
          offset += strlen(compare->name);

          while (operation > 0 && operations[operation-1]->precedence >= compare->precedence)
          {
            operator_t *consume = operations[--operation];
            expr_t *pair = expr_alloc();
            pair->type   = EXPR_OPCODE;
            pair->opcode = consume->opcode;
            pair->vals[pair->valc++] = arguments[--argument];
            pair->vals[pair->valc++] = arguments[--argument];
            arguments[argument++] = pair;
          }

          operations[operation++] = compare;
        }
      }

      if (!have_operator)
      {
        while (operation > 0)
        {
          operator_t *consume = operations[--operation];
          expr_t *pair = expr_alloc();
          pair->type   = EXPR_OPCODE;
          pair->opcode = consume->opcode;
          pair->vals[pair->valc++] = arguments[--argument];
          pair->vals[pair->valc++] = arguments[--argument];
          arguments[argument++] = pair;
        }

        ensure(argument == 1)
          errorf("unbalanced infix expression: %s", start);

        expr->vals[expr->valc++] = arguments[0];
        break;
      }
    }

    offset += skip_gap(&source[offset]);

    if (source[offset] == '=')
    {
      ensure (level == 0)
        errorf("unexpected assignment: %s", source);

      offset++;
      for (int i = 0; i < expr->valc; i++)
        expr->keys[i] = expr->vals[i];
      expr->keyc = expr->valc;
      expr->valc = 0;
      continue;
    }

    if (source[offset] == ',')
    {
      offset++;
      continue;
    }

    break;
  }

  push(expr);
  return offset;
}

void
process (expr_t *expr, int assign, int index)
{
  if (expr->args)
    process(expr->args, 0, 0);

  if (expr->type == EXPR_MULTI)
  {
    compile(OP_MARK);

    for (int i = 0; i < expr->valc; i++)
      process(expr->vals[i], 0, 0);

    for (int i = 0; i < expr->keyc; i++)
      process(expr->keys[i], 1, i);

    compile(OP_LIMIT)->offset = expr->results;
  }
  else
  if (expr->type == EXPR_VARIABLE)
  {
    code_t *code = compile(assign ? OP_ASSIGN_LIT: OP_FIND_LIT);
    code->ptr = expr->item;
    code->offset = index;
  }
  else
  if (expr->type == EXPR_LITERAL)
  {
    compile(OP_LIT)->ptr = expr->item;
  }
  else
  if (expr->type == EXPR_OPCODE)
  {
    for (int i = 0; i < expr->valc; i++)
      process(expr->vals[i], 0, 0);

    compile(expr->opcode);
  }
  else
  {
    ensure(0)
      errorf("unexpected expression type: %d", expr->type);
  }

  if (expr->call)
    compile(OP_CALL);

  expr_free(expr);
}

void
source (char *s)
{
  int offset = 0;

  int mark = depth();

  while (s[offset])
    offset += parse(&s[offset], 0, 0);

  errorf("depth %d", depth());

  for (int i = mark; i < depth(); i++)
    process(item(i)[0], 0, 0);
}
