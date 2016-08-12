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

enum {
  EXPR_MULTI=1,
  EXPR_VARIABLE,
  EXPR_LITERAL,
  EXPR_OPCODE,
  EXPR_IF,
  EXPR_WHILE,
  EXPR_FUNCTION,
  EXPR_RETURN,
  EXPR_BUILTIN,
  EXPR_VEC,
  EXPR_MAP,
  EXPR_FOR
};

#define RESULTS_DISCARD 0
#define RESULTS_FIRST 1
#define RESULTS_ALL -1

#define PARSE_GREEDY 1
#define PARSE_KEYVAL 2

#define PROCESS_ASSIGN (1<<0)
#define PROCESS_CHAIN (1<<1)
#define PROCESS_INDEX (1<<2)

int
isnamefirst (int c)
{
  return isalpha(c) || c == '_';
}

int
isname (int c)
{
  return isnamefirst(c) || isdigit(c);
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

int
peek (char *source, char *name)
{
  int len = strlen(name);
  return !strncmp(source, name, len) && !isname(source[len]);
}

expr_t*
expr_alloc ()
{
  expr_t *expr = heap_alloc(sizeof(expr_t));
  memset(expr, 0, sizeof(expr_t));
  return expr;
}

void
expr_keys_vals (expr_t *expr)
{
  if (!expr->keys)
  {
    expr->keys = vec_incref(vec_alloc());
    expr->vals = vec_incref(vec_alloc());
  }
}

void
expr_free (expr_t *expr)
{
  if (expr->keys) { expr->keys->count = 0; discard(expr->keys); }
  if (expr->vals) { expr->vals->count = 0; discard(expr->vals); }
  heap_free(expr);
}

typedef struct {
  char *name;
  int opcode;
} keyword_t;

keyword_t keywords[] = {
  { .name = "global", .opcode = OP_GLOBAL },
  { .name = "local", .opcode = OP_LOCAL },
  { .name = "true", .opcode = OP_TRUE },
  { .name = "false", .opcode = OP_FALSE },
  { .name = "nil", .opcode = OP_NIL },
};

int
parse_block (char *source, expr_t *expr)
{
  int length = 0;
  int found_end = 0;
  int offset = skip_gap(source);
  expr_keys_vals(expr);

  if (peek(&source[offset], "do"))
    offset += 2;

  while (source[offset])
  {
    if ((length = skip_gap(&source[offset])) > 0)
    {
      offset += length;
      continue;
    }
    if (peek(&source[offset], "end"))
    {
      offset += 3;
      found_end = 1;
      break;
    }

    offset += parse(&source[offset], RESULTS_DISCARD, PARSE_GREEDY);
    vec_push(expr->vals)[0] = pop();
  }

  ensure (found_end)
    errorf("expected keyword 'end': %s", source);

  return offset;
}

int
parse_branch (char *source, expr_t *expr)
{
  int length = 0;
  int found_else = 0;
  int found_end = 0;
  int offset = skip_gap(source);
  expr_keys_vals(expr);

  if (peek(&source[offset], "then"))
    offset += 4;

  while (source[offset])
  {
    if ((length = skip_gap(&source[offset])) > 0)
    {
      offset += length;
      continue;
    }
    if (peek(&source[offset], "else"))
    {
      offset += 4;
      found_else = 1;
      break;
    }
    if (peek(&source[offset], "end"))
    {
      offset += 3;
      found_end = 1;
      break;
    }

    offset += parse(&source[offset], RESULTS_DISCARD, PARSE_GREEDY);
    vec_push(expr->vals)[0] = pop();
  }

  if (found_else)
  {
    while (source[offset])
    {
      if ((length = skip_gap(&source[offset])) > 0)
      {
        offset += length;
        continue;
      }
      if (peek(&source[offset], "end"))
      {
        offset += 3;
        found_end = 1;
        break;
      }

      offset += parse(&source[offset], RESULTS_DISCARD, PARSE_GREEDY);
      vec_push(expr->keys)[0] = pop();
    }
  }

  ensure (found_end)
    errorf("expected keyword 'end': %s", source);

  if (expr->vals && expr->vals->count)
    ((expr_t*)(vec_get(expr->vals, expr->vals->count-1)[0]))->results = 1;

  if (expr->keys && expr->keys->count)
    ((expr_t*)(vec_get(expr->keys, expr->keys->count-1)[0]))->results = 1;

  return offset;
}

int
parse_arglist (char *source)
{
  int mark = depth();
  int offset = skip_gap(source);

  if (source[offset] == '(')
  {
    offset++;
    offset += skip_gap(&source[offset]);

    if (source[offset] != ')')
    {
      offset += parse(&source[offset], RESULTS_ALL, PARSE_GREEDY);
      offset += skip_gap(&source[offset]);
    }

    ensure (source[offset] == ')')
      errorf("expected closing paren: %s", source);
    offset++;
  }
  else
  {
    offset += parse(&source[offset], RESULTS_FIRST, PARSE_GREEDY);
  }

  if (depth() == mark)
    push(NULL);

  return offset;
}

int
parse_item (char *source)
{
  int length = 0;
  int offset = skip_gap(source);

  expr_t *expr = expr_alloc();
  expr->source = &source[offset];

  if (isnamefirst(source[offset]))
  {
    expr->type = EXPR_VARIABLE;
    length = str_skip(&source[offset], isname);

    int have_keyword = 0;

    for (int i = 0; i < sizeof(keywords) / sizeof(keyword_t) && !have_keyword; i++)
    {
      keyword_t *keyword = &keywords[i];

      if (peek(&source[offset], keyword->name))
      {
        expr->type = EXPR_OPCODE;
        expr->opcode = keyword->opcode;
        have_keyword = 1;
        offset += length;
      }
    }

    if (!have_keyword)
    {
      if (peek(&source[offset], "if"))
      {
        offset += 2;
        expr->type = EXPR_IF;

        // conditions
        offset += parse(&source[offset], RESULTS_FIRST, PARSE_GREEDY);
        expr->args = pop();

        // then block, optional else
        offset += parse_branch(&source[offset], expr);
      }
      else
      if (peek(&source[offset], "while"))
      {
        offset += 5;
        expr->type = EXPR_WHILE;

        // conditions
        offset += parse(&source[offset], RESULTS_FIRST, PARSE_GREEDY);
        expr->args = pop();

        // do block
        offset += parse_block(&source[offset], expr);
      }
      else
      if (peek(&source[offset], "for"))
      {
        offset += 3;
        expr->type = EXPR_FOR;
        expr_keys_vals(expr);

        // key[,val] local variable names
        offset += skip_gap(&source[offset]);
        ensure(isnamefirst(source[offset]))
          errorf("expected variable: %s", &source[offset]);

        length = str_skip(&source[offset], isname);
        vec_push(expr->keys)[0] = substr(&source[offset], 0, length);
        offset += length;

        offset += skip_gap(&source[offset]);
        if (source[offset] == ',')
        {
          offset++;
          length = str_skip(&source[offset], isname);
          vec_push(expr->keys)[0] = substr(&source[offset], 0, length);
          offset += length;
        }

        offset += skip_gap(&source[offset]);
        if (peek(&source[offset], "in")) offset += 2;

        // iterable
        offset += parse(&source[offset], RESULTS_FIRST, PARSE_GREEDY);
        expr->args = pop();

        // do block
        offset += parse_block(&source[offset], expr);
      }
      else
      if (peek(&source[offset], "function"))
      {
        offset += 8;
        expr->type = EXPR_FUNCTION;
        expr_keys_vals(expr);

        offset += skip_gap(&source[offset]);

        // optional function name
        if (isnamefirst(source[offset]))
        {
          length = str_skip(&source[offset], isname);
          expr->item = substr(&source[offset], 0, length);
          offset += length;
        }

        offset += skip_gap(&source[offset]);

        // argument locals list
        if (source[offset] == '(')
        {
          offset++;

          while (source[offset])
          {
            if ((length = skip_gap(&source[offset])) > 0)
            {
              offset += length;
              continue;
            }
            if (source[offset] == ',')
            {
              offset++;
              continue;
            }
            if (source[offset] == ')')
            {
              offset++;
              break;
            }

            ensure(isnamefirst(source[offset]))
              errorf("expected parameter: %s", &source[offset]);

            length = str_skip(&source[offset], isname);

            expr_t *param = expr_alloc();
            param->type = EXPR_VARIABLE;
            param->item = substr(&source[offset], 0, length);
            vec_push(expr->keys)[0] = param;

            offset += length;
          }
        }

        // do block
        offset += parse_block(&source[offset], expr);
      }
      else
      if (peek(&source[offset], "return"))
      {
        offset += 6;
        expr->type = EXPR_RETURN;

        offset += skip_gap(&source[offset]);

        if (!peek(&source[offset], "end"))
        {
          offset += parse(&source[offset], RESULTS_ALL, PARSE_GREEDY);
          expr->args = pop();
        }
      }
      else
      if (peek(&source[offset], "break"))
      {
        offset += 5;
        expr->type = EXPR_OPCODE;
        expr->opcode = OP_BREAK;
      }
      else
      if (peek(&source[offset], "continue"))
      {
        offset += 8;
        expr->type = EXPR_OPCODE;
        expr->opcode = OP_CONTINUE;
      }
      else
      if (peek(&source[offset], "coroutine"))
      {
        offset += 9;
        expr->type = EXPR_BUILTIN;
        expr->opcode = OP_COROUTINE;
        offset += parse_arglist(&source[offset]);
        expr->args = pop();
        expr->results = 1;
      }
      else
      if (peek(&source[offset], "yield"))
      {
        offset += 5;
        expr->type = EXPR_BUILTIN;
        expr->opcode = OP_YIELD;
        offset += parse_arglist(&source[offset]);
        expr->args = pop();
        expr->results = -1;
      }
      else
      if (peek(&source[offset], "resume"))
      {
        offset += 6;
        expr->type = EXPR_BUILTIN;
        expr->opcode = OP_RESUME;
        offset += parse_arglist(&source[offset]);
        expr->args = pop();
        expr->results = -1;
      }
      else
      if (peek(&source[offset], "not"))
      {
        offset += 3;
        expr->type = EXPR_OPCODE;
        expr->opcode = OP_NOT;
        offset += parse(&source[offset], RESULTS_FIRST, PARSE_GREEDY);
        expr->args = pop();
        expr->results = 1;
      }
      else
      {
        expr->item = substr(&source[offset], 0, length);
        offset += length;
      }
    }
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
  if (source[offset] == '[' && source[offset+1] == '[')
  {
    expr->type = EXPR_LITERAL;

    char *start = &source[offset+2];
    char *end = start;

    while (*end && !(end[0] == ']' && end[1] == ']'))
      end = strchr(end, ']');

    ensure (end[0] == ']' && end[1] == ']')
      errorf("expected closing bracket: %s", &source[offset]);

    expr->item = substr(start, 0, end - start);
    offset += end - &source[offset] + 2;
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
  if (source[offset] == '#')
  {
    offset++;
    expr->type = EXPR_OPCODE;
    expr->opcode = OP_COUNT;
    offset += parse(&source[offset], RESULTS_FIRST, PARSE_GREEDY);
    expr->args = pop();
  }
  else
  if (source[offset] == '[')
  {
    offset++;
    expr->type = EXPR_VEC;
    offset += parse(&source[offset], RESULTS_ALL, PARSE_GREEDY);
    expr->args = pop();
    offset += skip_gap(&source[offset]);

    ensure (source[offset] == ']')
      errorf("expected closing bracket: %s", source);
    offset++;
  }
  else
  if (source[offset] == '{')
  {
    offset++;
    expr->type = EXPR_MAP;
    expr_keys_vals(expr);

    while (source[offset])
    {
      if ((length = skip_gap(&source[offset])) > 0)
      {
        offset += length;
        continue;
      }
      if (source[offset] == '}')
      {
        offset++;
        break;
      }
      if (source[offset] == ',')
      {
        offset++;
        continue;
      }
      offset += parse(&source[offset], RESULTS_DISCARD, PARSE_KEYVAL);
      vec_push(expr->vals)[0] = pop();
    }
  }
  else
  {
    ensure(0)
      errorf("what: %s", &source[offset]);
  }

  expr_t *prev = expr;

  while (source[offset])
  {
    if ((length = skip_gap(&source[offset])) > 0)
    {
      offset += length;
      continue;
    }

    if (source[offset] == '(')
    {
      prev->call = 1;
      offset += parse_arglist(&source[offset]);
      prev->args = pop();
      break;
    }

    if (source[offset] == '[')
    {
      offset++;
      offset += parse_item(&source[offset]);
      prev->index = pop();
      prev = prev->index;
      offset += skip_gap(&source[offset]);
      ensure(source[offset] == ']')
        errorf("expected closing bracket: %s", &source[offset]);
      offset++;
      continue;
    }

    if (source[offset] == '.' && source[offset+1] != '.')
    {
      offset++;
      offset += parse_item(&source[offset]);
      prev->chain = pop();
      prev = prev->index;
      break;
    }

    break;
  }
  push(expr);
  return offset;
}

typedef struct {
  char *name;
  int precedence;
  int opcode;
  int argc;
} operator_t;

operator_t operators[] = {
  { .name = "and", .precedence = 0, .opcode = OP_AND    },
  { .name = "or",  .precedence = 0, .opcode = OP_OR     },
  { .name = "==",  .precedence = 1, .opcode = OP_EQ     },
  { .name = "!=",  .precedence = 1, .opcode = OP_NE     },
  { .name = ">",   .precedence = 1, .opcode = OP_GT     },
  { .name = ">=",  .precedence = 1, .opcode = OP_GTE    },
  { .name = "<",   .precedence = 1, .opcode = OP_LT     },
  { .name = "<=",  .precedence = 1, .opcode = OP_LTE    },
  { .name = "~",   .precedence = 1, .opcode = OP_MATCH  },
  { .name = "..",  .precedence = 2, .opcode = OP_CONCAT },
  { .name = "+",   .precedence = 3, .opcode = OP_ADD    },
  { .name = "-",   .precedence = 3, .opcode = OP_SUB    },
  { .name = "*",   .precedence = 4, .opcode = OP_MUL    },
  { .name = "/",   .precedence = 4, .opcode = OP_DIV    },
  { .name = "%",   .precedence = 4, .opcode = OP_MOD    },
};

int
parse (char *source, int results, int mode)
{
  int length = 0;
  int offset = skip_gap(source);

  expr_t *expr = expr_alloc();
  expr->type = EXPR_MULTI;
  expr->results = results;
  expr_keys_vals(expr);

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

      if (source[offset] == '(')
      {
        offset += parse_arglist(&source[offset]);
        arguments[argument++] = pop();
        arguments[argument-1]->results = 1;
      }
      else
      {
        offset += parse_item(&source[offset]);
        arguments[argument++] = pop();
      }

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
            expr_keys_vals(pair);
            vec_push(pair->vals)[0] = arguments[argument-2];
            vec_push(pair->vals)[0] = arguments[argument-1];
            argument -= 2;
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
          expr_keys_vals(pair);
          vec_push(pair->vals)[0] = arguments[argument-2];
          vec_push(pair->vals)[0] = arguments[argument-1];
          argument -= 2;
          arguments[argument++] = pair;
        }

        ensure(argument == 1)
          errorf("unbalanced infix expression: %s", start);

        vec_push(expr->vals)[0] = arguments[0];
        break;
      }
    }

    offset += skip_gap(&source[offset]);

    if (source[offset] == '=')
    {
      ensure (expr->vals->count > 0)
        errorf("missing assignment name: %s", source);

      ensure (expr->keys->count == 0)
        errorf("invalid nested assignment: %s", source);

      offset++;
      for (int i = 0; i < expr->vals->count; i++)
        vec_push(expr->keys)[0] = vec_get(expr->vals, i)[0];
      expr->vals->count = 0;
      continue;
    }

    if (source[offset] == ',' && mode == PARSE_GREEDY)
    {
      offset++;
      continue;
    }

    break;
  }

  ensure(expr->vals->count > 0)
    errorf("missing assignment value: %s", source);

  push(expr);
  return offset;
}

void
process (expr_t *expr, int flags, int index)
{
  int flag_assign = flags & PROCESS_ASSIGN ? 1:0;
  int flag_chain  = flags & PROCESS_CHAIN  ? 1:0;
  int flag_index  = flags & PROCESS_INDEX  ? 1:0;

  // a multi-part expression: a[,b...] = expr[,expr...]
  if (expr->type == EXPR_MULTI)
  {
    ensure(!expr->args);
    ensure(expr->vals && expr->vals->count);

    // stack frame
    compile(OP_MARK);

    // stream the values onto the stack
    if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
      process(vec_get(expr->vals, i)[0], 0, 0);

    // OP_SET|OP_ASSIGN index values from the bottom of the current stack frame
    if (expr->keys) for (int i = 0; i < expr->keys->count; i++)
      process(vec_get(expr->keys, i)[0], PROCESS_ASSIGN, i);

    // end stack frame
    compile(OP_LIMIT)->offset = expr->results;
  }
  else
  if (expr->type == EXPR_VARIABLE)
  {
    ensure(!expr->keys && !expr->vals);

    // if we're assigning with chained expressions, only OP_SET|OP_ASSIGN the last one
    int assign = flag_assign && !expr->chain;

    // function or method call, optionally chained
    if (expr->call)
    {
      if (flag_chain)
        compile(OP_SHUNT);

      if (expr->args)
        process(expr->args, 0, 0);

      if (flag_chain)
        compile(OP_SHIFT);

      compile(OP_LIT)->ptr = expr->item;

      int opcode = assign ? (flag_chain || flag_index
        ? OP_SET: OP_ASSIGN) : (flag_chain || flag_index ? OP_GET: OP_FIND);

      compile(opcode)->offset = index;

      compile(OP_CALL);
    }
    else
    // variable reference, optionally chained
    {
      compile(OP_LIT)->ptr = expr->item;

      if (flag_index)
        compile(OP_FIND);

      int opcode = assign ? (flag_chain || flag_index
        ? OP_SET: OP_ASSIGN) : (flag_chain || flag_index ? OP_GET: OP_FIND);

      compile(opcode)->offset = index;
    }
  }
  else
  // string or number literal, optionally part of array chain a[b]["c"]
  if (expr->type == EXPR_LITERAL)
  {
    ensure(!expr->args && !expr->keys && !expr->vals);

    char *dollar = is_str(expr->item) ? strchr(expr->item, '$'): NULL;

    if (dollar && dollar < (char*)expr->item + strlen(expr->item) - 1)
    {
      char *str = expr->item;
      char *left = str;
      char *right = str;

      compile(OP_LIT)->ptr = strf("");

      while ((right = strchr(left, '$')) && right && *right)
      {
        char *start = right+1;
        char *finish = start;
        int length = 0;

        if (*start == '{')
        {
          start++;
          char *rbrace = strchr(start, '}');
          ensure(rbrace)
            errorf("missing closing brace: %s", right);
          length = rbrace-start;
          finish = rbrace+1;
        }
        else
        {
          length = str_skip(start, isname);
          finish = &start[length];
        }

        compile(OP_LIT)->ptr = substr(left, 0, right-left+(length ? 0:1));
        compile(OP_CONCAT);

        left = finish;

        if (length)
        {
          char *sub = substr(start, 0, length);
          parse(sub, RESULTS_FIRST, PARSE_GREEDY);
          process(pop(), 0, 0);
          discard(sub);
        }

        compile(OP_CONCAT);
      }

      compile(OP_LIT)->ptr = substr(left, 0, strlen(left));

      compile(OP_CONCAT);
      discard(expr->item);
    }
    else
    {
      compile(OP_LIT)->ptr = expr->item;
    }

    if (flag_index)
      compile(flag_assign ? OP_SET: OP_GET);
  }
  else
  // inline opcode
  if (expr->type == EXPR_OPCODE)
  {
    if (expr->args)
      process(expr->args, 0, 0);

    if (expr->opcode == OP_AND || expr->opcode == OP_OR)
    {
      process(vec_get(expr->vals, 0)[0], 0, 0);
      code_t *jump = compile(expr->opcode);
      process(vec_get(expr->vals, 1)[0], 0, 0);
      jump->offset = code_count;
    }
    else
    {
      if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
        process(vec_get(expr->vals, i)[0], 0, 0);

      compile(expr->opcode);
    }
  }
  else
  // a built-in function-like keyword with arguments
  if (expr->type == EXPR_BUILTIN)
  {
    ensure(!expr->keys && !expr->vals);

    compile(OP_MARK);

    if (expr->args)
      process(expr->args, 0, 0);

    compile(expr->opcode);
    compile(OP_LIMIT)->offset = expr->results;
  }
  else
  // if expression (returns a value for ternary style)
  if (expr->type == EXPR_IF)
  {
    ensure(expr->vals);

    // conditions
    if (expr->args)
      process(expr->args, 0, 0);

    // if false, jump to else/end
    code_t *jump = compile(OP_JFALSE);
    compile(OP_DROP);

    // success block
    if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
      process(vec_get(expr->vals, i)[0], 0, 0);

    // optional failure block
    if (expr->keys && expr->keys->count)
    {
      // jump success path past failure block
      code_t *jump2 = compile(OP_JMP);
      jump->offset = code_count;
      compile(OP_DROP);

      // failure block
      for (int i = 0; i < expr->keys->count; i++)
        process(vec_get(expr->keys, i)[0], 0, 0);

      jump2->offset = code_count;
    }
    else
    {
      jump->offset = code_count;
    }
  }
  else
  // while ... do ... end
  if (expr->type == EXPR_WHILE)
  {
    ensure(expr->vals);

    compile(OP_MARK);
    code_t *loop = compile(OP_LOOP);
    int begin = code_count;

    // condition(s)
    if (expr->args)
      process(expr->args, 0, 0);

    // if false, jump to end
    code_t *jump = compile(OP_JFALSE);
    compile(OP_DROP);

    // do ... end
    if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
      process(vec_get(expr->vals, i)[0], 0, 0);

    // clean up
    compile(OP_JMP)->offset = begin;
    jump->offset = code_count;
    loop->offset = code_count;
    compile(OP_UNLOOP);
    compile(OP_LIMIT);
  }
  else
  // for ... in ... do ... end
  if (expr->type == EXPR_FOR)
  {
    compile(OP_MARK);

    // the iterable
    if (expr->args)
      process(expr->args, 0, 0);

    // loop counter
    compile(OP_LIT)->ptr = to_int(0);

    compile(OP_MARK);
    code_t *loop = compile(OP_LOOP);

    int begin = code_count;

    code_t *jump = compile(OP_FOR);
    // OP_FOR expects a vector with key[,val] variable names
    jump->ptr = expr->keys;
    expr->keys = NULL;

    // do block
    if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
      process(vec_get(expr->vals, i)[0], 0, 0);

    // clean up
    compile(OP_JMP)->offset = begin;
    jump->offset = code_count;
    loop->offset = code_count;
    compile(OP_UNLOOP);
    compile(OP_LIMIT);
    compile(OP_LIMIT);
  }
  else
  // function with optional name assignment
  if (expr->type == EXPR_FUNCTION)
  {
    ensure(!expr->args);

    compile(OP_MARK);
    code_t *entry = compile(OP_LIT);

    if (expr->item)
    {
      code_t *name = compile(OP_ASSIGN_LIT);
      name->ptr = expr->item;
      name->offset = 0;
    }

    code_t *jump = compile(OP_JMP);
    entry->ptr = to_sub(code_count);

    if (expr->keys) for (int i = 0; i < expr->keys->count; i++)
      process(vec_get(expr->keys, i)[0], PROCESS_ASSIGN, i);

    if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
      process(vec_get(expr->vals, i)[0], 0, 0);

    // if an explicit return expression is used, these instructions
    // will be dead code
    compile(OP_REPLY);
    compile(OP_RETURN);
    jump->offset = code_count;

    compile(OP_LIMIT)->offset = 1;
  }
  else
  // return 0 or more values
  if (expr->type == EXPR_RETURN)
  {
    compile(OP_REPLY);

    if (expr->args)
      process(expr->args, 0, 0);

    compile(OP_RETURN);
  }
  else
  // literal vector [1,2,3]
  if (expr->type == EXPR_VEC)
  {
    compile(OP_MARK);

    if (expr->args)
      process(expr->args, 0, 0);

    compile(OP_LITSTACK);
    compile(OP_LIMIT)->offset = 1;
  }
  else
  // literal map { a = 1, b = 2, c = nil }
  if (expr->type == EXPR_MAP)
  {
    compile(OP_MARK);
    compile(OP_SCOPE);
    compile(OP_SMUDGE);

    if (expr->args)
      process(expr->args, 0, 0);

    if (expr->vals) for (int i = 0; i < expr->vals->count; i++)
      process(vec_get(expr->vals, i)[0], 0, 0);

    compile(OP_LITSCOPE);
    compile(OP_LIMIT)->offset = 1;
  }
  else
  {
    ensure(0)
      errorf("unexpected expression type: %d", expr->type);
  }

  if (expr->chain)
    process(expr->chain, PROCESS_CHAIN | (flag_assign ? PROCESS_ASSIGN: 0), 0);

  if (expr->index)
    process(expr->index, PROCESS_INDEX | (flag_assign ? PROCESS_ASSIGN: 0), 0);

  expr_free(expr);
}

void
source (char *s)
{
  int offset = 0;

  int mark = depth();

  while (s[offset])
    offset += parse(&s[offset], RESULTS_DISCARD, PARSE_GREEDY);

  for (int i = mark; i < depth(); i++)
    process(item(i)[0], 0, 0);
}
