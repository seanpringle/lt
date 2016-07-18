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

int
isname (int c)
{
  return isalpha(c) || isdigit(c) || c == '_';
}

int
isseparator (int c)
{
  return isspace(c) || c == ',';
}

int
islf (int c)
{
  return c == '\n';
}

int
peek (char *source, char *name)
{
  int len = strlen(name);
  return strncmp(source, name, len) == 0 && !isname(source[len]);
}

int
peek_control (char *source)
{
  return
       peek(source, "if")
    || peek(source, "then")
    || peek(source, "else")
    || peek(source, "while")
    || peek(source, "for")
    || peek(source, "do")
    || peek(source, "end")
    || peek(source, "function")
    || peek(source, "and")
    || peek(source, "or")
    || peek(source, "not");
}

int
skip_comment (char *source)
{
  int offset = str_skip(&source[0], isspace);

  while (source[offset] == '-' && source[offset+1] == '-')
  {
    offset += str_scan(&source[offset], islf);
    offset += str_skip(&source[offset], islf);
  }
  return offset;
}

int parse (char*);
int parse_control (char*);
int parse_argument (char*, int level);

#define BLOCK_FUNC 1
#define BLOCK_BRANCH 2
#define BLOCK_WHILE 3
#define BLOCK_FOR 4

int
parse_control (char *source)
{
  int offset = str_skip(&source[0], isspace);
  offset += skip_comment(&source[offset]);
  char *start = source;

  if (peek(&source[offset], "end"))
  {
    offset += 3;

    int type = pop_int();
    int branch = pop_int();

    switch (type)
    {
      case BLOCK_FUNC:
        compile(OP_RETURN);
        code[branch].offset = code_count;
        break;

      case BLOCK_BRANCH:
        code[branch].offset = code_count;
        break;

      case BLOCK_WHILE:
        compile(OP_UNSTACK);
        compile(OP_JMP)->offset = pop_int();
        code[branch].offset = code_count;
        break;

      case BLOCK_FOR:
        compile(OP_UNSTACK);
        compile(OP_JMP)->offset = pop_int();
        code[branch].offset = code_count;
        compile(OP_DROP);
        break;
    }
  }
  else
  if (peek(&source[offset], "return"))
  {
    offset += 6;
    int argc = 0;
    for (;;)
    {
      offset += parse(&source[offset]);
      argc++;

      offset += str_skip(&source[offset], isspace);
      if (source[offset] != ',') break;
      offset++;
    }
    compile(OP_RETURN)->offset = argc;
  }
  else
  if (peek(&source[offset], "function"))
  {
    offset += 8;
    offset += str_skip(&source[offset], isspace);

    int length = 0;
    char *name = NULL;

    if (isalpha(source[offset]))
    {
      length = str_skip(&source[offset], isname);
      name = substr(&source[offset], 0, length);

      offset += length;
      offset += str_skip(&source[offset], isspace);

      compile(OP_LIT)->ptr = to_int(code_count+3);
      compile(OP_LIT)->ptr = name;
      compile(OP_DEFINE);
    }
    else
    {
      compile(OP_LIT)->ptr = to_int(code_count+1);
    }

    ensure(source[offset] == '(')
      errorf("expected arguments: %s", &source[offset]);

    offset++;

    compile(OP_JMP);
    int mark = depth();
    push_int(code_count-1);
    push_int(BLOCK_FUNC);

    char *args[32];
    int arg_count = 0;

    for (;;)
    {
      offset += str_skip(&source[offset], isseparator);
      if (source[offset] == ')') { offset++; break; }

      ensure(isalpha(source[offset]))
        errorf("expected name: %s", &source[offset]);

      start = &source[offset];
      length = str_skip(&source[offset], isname);

      args[arg_count++] = substr(start, 0, length);
      offset += length;
    }

    compile(OP_DEFNIL)->offset = arg_count;

    for (int i = arg_count-1; i >= 0; i--)
    {
      compile(OP_LIT)->ptr = args[i];
      compile(OP_DEFINE);
    }

    while (depth() > mark)
      offset += parse(&source[offset]);
  }
  else
  if (peek(&source[offset], "if"))
  {
    offset += 2;
    offset += parse(&source[offset]);
    compile(OP_TEST);
  }
  else
  if (peek(&source[offset], "while"))
  {
    offset += 5;
    int mark = depth();
    push_int(code_count);
    offset += parse(&source[offset]);
    compile(OP_TEST);
    push_int(BLOCK_WHILE);

    while (depth() > mark)
      offset += parse(&source[offset]);
  }
  else
  if (peek(&source[offset], "not"))
  {
    offset += 3;
    offset += parse(&source[offset]);
    compile(OP_NOT);
  }
  else
  if (peek(&source[offset], "and"))
  {
    offset += 3;
    int co = compile(OP_JZ) - code;
    offset += parse(&source[offset]);
    compile(OP_TEST);
    code[co].offset = code_count;
  }
  else
  if (peek(&source[offset], "or"))
  {
    offset += 2;
    int co = compile(OP_JNZ) - code;
    offset += parse(&source[offset]);
    compile(OP_TEST);
    code[co].offset = code_count;
  }
  else
  if (peek(&source[offset], "then"))
  {
    offset += 4;
    compile(OP_JZ);
    push_int(code_count-1);
    push_int(BLOCK_BRANCH);
  }
  else
  if (peek(&source[offset], "do"))
  {
    offset += 2;
    switch (pop_int())
    {
      case BLOCK_WHILE:
        compile(OP_JZ);
        push_int(code_count-1);
        push_int(BLOCK_WHILE);
        break;

      case BLOCK_FOR:
        push_int(code_count-1);
        push_int(BLOCK_FOR);
        break;
    }
    compile(OP_STACK);
  }
  else
  if (peek(&source[offset], "else"))
  {
    offset += 4;
    compile(OP_JMP);

    pop_int();

    int branch = pop_int();
    code[branch].offset = code_count;

    push_int(code_count-1);
    push_int(BLOCK_BRANCH);
  }
  else
  if (peek(&source[offset], "for"))
  {
    offset += 3;

    int length;
    char *key = NULL;

    offset += str_skip(&source[offset], isspace);

    length = str_skip(&source[offset], isname);
    key = substr(&source[offset], 0, length);

    offset += length;
    offset += str_skip(&source[offset], isspace);

    if (peek(&source[offset], "in"))
      offset += 2;

    offset += parse(&source[offset]);
    compile(OP_LIT)->ptr = to_int(0);

    push_int(code_count);
    compile(OP_FOR)->ptr = key;
    push_int(BLOCK_FOR);
  }
  else
  {
    stacktrace();
    abort();
  }
  return offset;
}

int
parse_assign (char *source, int level)
{
  int offset = 0;

  offset += str_skip(&source[offset], isspace);

  if (source[offset] == '=')
  {
    offset++;

    code_t *last = hindsight(-1);
    if (!level && last->op == OP_LIT)
    {
      void *ptr = last->ptr;
      code_count--;
      offset += parse(&source[offset]);
      compile(OP_ASSIGN_LIT)->ptr = ptr;
    }
    else
    {
      offset += parse(&source[offset]);
      compile(level ? OP_SET: OP_ASSIGN);
    }
  }
  else
  {
    compile(level ? OP_GET: OP_FIND);
  }

  return offset;
}

int
parse_argument (char *source, int level)
{
  int offset = str_skip(&source[0], isspace);
  offset += skip_comment(&source[offset]);

  ensure(!peek_control(&source[offset]))
    errorf("expected argument: %s", &source[offset]);

  if (isalpha(source[offset]))
  {
    char *start = &source[offset];
    int length = str_skip(&source[offset], isname);
    char *name = substr(start, 0, length);
    offset += length;
    offset += str_skip(&source[offset], isspace);

    if (!strcmp(name, "global"))
    {
      compile(OP_GLOBAL);
    }
    else
    if (!strcmp(name, "local"))
    {
      compile(OP_LOCAL);
    }
    else
    if (!strcmp(name, "string"))
    {
      compile(OP_STRING);
    }
    else
    if (!strcmp(name, "array"))
    {
      compile(OP_ARRAY);
    }
    else
    if (!strcmp(name, "table"))
    {
      compile(OP_TABLE);
    }
    else
    if (!strcmp(name, "nil"))
    {
      compile(OP_NIL);
    }
    else
    if (!strcmp(name, "true"))
    {
      compile(OP_TRUE);
    }
    else
    if (!strcmp(name, "false"))
    {
      compile(OP_FALSE);
    }
    else
    if (source[offset] == '(')
    {
      offset++;

      if (level > 0)
      {
        compile(OP_LIT)->ptr = name;
        compile(OP_GET);
      }

      compile(OP_STACK);

      for (;;)
      {
        offset += str_skip(&source[offset], isseparator);
        if (source[offset] == ')') { offset++; break; }
        offset += parse(&source[offset]);
      }

      compile(OP_SCOPE);

      if (level > 0)
      {
        compile(OP_CALL);
      }
      else
      {
        compile(OP_CALL_LIT)->ptr = name;
      }

      compile(OP_UNSCOPE);
      compile(OP_UNSTACK);
    }
    else
    {
      compile(OP_LIT)->ptr = name;
      offset += str_skip(&source[offset], isspace);
      offset += parse_assign(&source[offset], level);
    }

    offset += str_skip(&source[offset], isspace);

    if (source[offset] == '.')
    {
      offset++;
      offset += parse_argument(&source[offset], level+1);
      //offset += parse_assign(&source[offset], level);
    }

    offset += str_skip(&source[offset], isspace);

    if (source[offset] == '[')
    {
      offset++;
      offset += parse(&source[offset]);
      offset += str_skip(&source[offset], isspace);
      ensure(source[offset] == ']')
        errorf("expected ]: %s", &source[offset]);
      offset++;
      offset += parse_assign(&source[offset], level+1);
    }
  }
  else
  if (source[offset] == '[')
  {
    offset++;
    compile(OP_STACK);

    while (source[offset])
    {
      offset += str_skip(&source[offset], isseparator);
      if (source[offset] == ']') { offset++; break; }
      offset += parse(&source[offset]);
    }

    compile(OP_LITSTACK);
  }
  else
  if (source[offset] == '{')
  {
    offset++;
    compile(OP_SCOPE);
    compile(OP_SMUDGE);
    compile(OP_STACK);

    while (source[offset])
    {
      offset += str_skip(&source[offset], isseparator);
      if (source[offset] == '}') { offset++; break; }
      offset += parse(&source[offset]);
      compile(OP_DROP);
    }

    compile(OP_UNSTACK);
    compile(OP_LITSCOPE);
  }
  else
  if (source[offset] == '"')
  {
    char *end = NULL;
    compile(OP_LIT)->ptr = str_unquote(&source[offset], &end);
    offset += end - &source[offset];
  }
  else
  if (source[offset] == '#')
  {
    offset++;
    offset += parse(&source[offset]);
    compile(OP_COUNT);
  }
  else
  if (source[offset])
  {
    char *start = &source[offset];
    char *a = NULL, *b = NULL;
    int64_t ni = strtoll(start, &a, 0);
    double nd = strtod(start, &b);

    ensure(a > start || b > start)
      errorf("what: %s", start);

    compile(OP_LIT)->ptr = b > a ? (void*)to_dbl(nd): (void*)to_int(ni);
    offset += (b > a ? b: a)-start;
  }

  return offset;
}

int
parse (char *source)
{
  int offset = str_skip(&source[0], isspace);
  offset += skip_comment(&source[offset]);

  if (peek_control(&source[offset]))
  {
    offset += parse_control(&source[offset]);
    return offset;
  }

  int greedy = 1;

  struct token {
    int operation;
    int precedence;
  };

  struct token tokens[32];
  int token_count = 0;

  while (greedy)
  {
    greedy = 0;

    offset += str_skip(&source[offset], isspace);
    offset += skip_comment(&source[offset]);

    if (peek_control(&source[offset])) break;
    if (source[offset] == ')') break;

    if (source[offset] == '(')
    {
      offset++;
      offset += parse(&source[offset]);
      ensure(source[offset] == ')')
        errorf("expected closing paren: %s", &source[offset]);
      offset++;
    }
    else
    {
      offset += parse_argument(&source[offset], 0);
    }

    offset += str_skip(&source[offset], isspace);
    offset += skip_comment(&source[offset]);

    struct operator {
      char *name;
      int operation;
      int precedence;
    };

    struct operator operators[] = {
      { .name = "==", .operation = OP_EQ,     .precedence = 0 },
      { .name = "<=", .operation = OP_LTE,    .precedence = 0 },
      { .name = ">=", .operation = OP_GTE,    .precedence = 0 },
      { .name = "<",  .operation = OP_LT,     .precedence = 0 },
      { .name = ">",  .operation = OP_GT,     .precedence = 0 },
      { .name = "..", .operation = OP_CONCAT, .precedence = 0 },
      { .name = "~",  .operation = OP_MATCH,  .precedence = 0 },
      { .name = "+",  .operation = OP_ADD,    .precedence = 1 },
      { .name = "-",  .operation = OP_SUB,    .precedence = 1 },
      { .name = "*",  .operation = OP_MUL,    .precedence = 2 },
      { .name = "/",  .operation = OP_DIV,    .precedence = 2 },
    };

    if (!ispunct(source[offset])) break;

    for (int i = 0; i < sizeof(operators) / sizeof(struct operator); i++)
    {
      struct operator *op = &operators[i];

      if (!strncmp(&source[offset], op->name, strlen(op->name)))
      {
        offset += strlen(op->name);

        while (token_count > 0 && tokens[token_count-1].precedence >= op->precedence)
          compile(tokens[--token_count].operation);

        tokens[token_count].operation = op->operation;
        tokens[token_count].precedence = op->precedence;
        token_count++;

        greedy = 1;
        break;
      }
    }
  }

  while (token_count > 0)
    compile(tokens[--token_count].operation);

  return offset;
}
