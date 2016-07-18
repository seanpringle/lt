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

#include <pcre.h>

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

  char *buffer = malloc(strlen(subject)+1);

  for (int i = 0; i < matches; i++)
  {
    int offset = ovector[2*i];
    int length = ovector[2*i+1] - offset;
    memmove(buffer, subject+offset, length);
    buffer[length] = 0;
    push(substr(buffer, 0, length));
  }

  free(buffer);

  if (extra)
    pcre_free(extra);
  pcre_free(re);
}
