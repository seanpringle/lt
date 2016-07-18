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
#include "str.h"
#include "vec.h"
#include "map.h"
#include "lt.h"

uint32_t
str_djb_hash (const char *str)
{
  uint32_t hash = 5381;
  for (int i = 0; str[i]; hash = hash * 33 + str[i++]);
  return hash;
}

char*
strf (char *pattern, ...)
{
  char *result = NULL;
  va_list args;
  char buffer[8];

  va_start(args, pattern);
  int len = vsnprintf(buffer, sizeof(buffer), pattern, args);
  va_end(args);

  if (len > -1 && (result = arena_alloc(strs, len+1)) && result)
  {
    str_count++;
    va_start(args, pattern);
    vsnprintf(result, len+1, pattern, args);
    va_end(args);
  }
  return result;
}

char*
substr (char *start, int offset, int length)
{
  char *buffer = arena_alloc(strs, length+1);

  ensure(buffer)
  {
    errorf("arena_alloc strs");
    stacktrace();
  }
  strncpy(buffer, start+offset, length);
  buffer[length] = 0;
  str_count++;
  return buffer;
}

char*
str_quote (char *str)
{
  char *res = arena_alloc(strs, strlen(str)*2+3);
  char *rp = res, *sp = str;

  *rp++ = '"';

  while (sp && *sp)
  {
    int c = *sp++;
    if (c == '"') { *rp++ = '\\'; }
    else if (c == '\\') { *rp++ = '\\'; }
    else if (c == '\a') { *rp++ = '\\'; c = 'a'; }
    else if (c == '\b') { *rp++ = '\\'; c = 'b'; }
    else if (c == '\f') { *rp++ = '\\'; c = 'f'; }
    else if (c == '\n') { *rp++ = '\\'; c = 'n'; }
    else if (c == '\r') { *rp++ = '\\'; c = 'r'; }
    else if (c == '\t') { *rp++ = '\\'; c = 't'; }
    else if (c == '\v') { *rp++ = '\\'; c = 'v'; }
    *rp++ = c;
  }

  *rp++ = '"';
  *rp = 0;
  return res;
}

char*
str_unquote (char *str, char **err)
{
  char *res = arena_alloc(strs, strlen(str)+1);
  char *rp = res, *sp = str;

  sp++;

  while (sp && *sp)
  {
    int c = *sp++;
    if (c == '"') break;

    if (c == '\\')
    {
           if (*sp == 'a') c = '\a';
      else if (*sp == 'b') c = '\b';
      else if (*sp == 'f') c = '\f';
      else if (*sp == 'n') c = '\n';
      else if (*sp == 'r') c = '\r';
      else if (*sp == 't') c = '\t';
      else if (*sp == 'v') c = '\v';
      else c = *sp++;
    }
    *rp++ = c;
  }
  *rp = 0;

  if (err)
    *err = sp;

  return res;
}

int
str_skip (char *source, strcb cb)
{
  int offset = 0;
  while (source[offset] && cb(source[offset])) offset++;
  return offset;
}

int
str_scan (char *source, strcb cb)
{
  int offset = 0;
  while (source[offset] && !cb(source[offset])) offset++;
  return offset;
}
