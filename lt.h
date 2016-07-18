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

#define ensure(x) for ( ; !(x) ; exit(EXIT_FAILURE) )
#define errorf(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); fflush(stderr); } while(0)
#define abort() ensure(0) errorf("abort %s %d %s", __FILE__, __LINE__, __func__)

void *bool_true;
void *bool_false;

#define FLAG_TRUE (1<<0)

#define KB (1024)
#define MB (KB*KB)

typedef struct {
  int op;
  int offset;
  void *ptr;
} code_t;

typedef void (*opcb)();

typedef struct {
  const char *name;
  opcb func;
} func_t;

struct wrapper {
  int op;
  int arguments;
  int results;
  char *name;
};

int is_bool (void*);
int is_int (void*);
int is_dbl (void*);
int is_str (void*);
int is_vec (void*);
int is_map (void*);
char* to_char (void*);
void* to_bool(int);
int get_bool(void*);
int64_t* to_int(int64_t);
int64_t get_int(void*);
double* to_dbl(double);
double get_dbl(void*);
int discard (void*);
int equal (void*,void*);
void* copy (void*);
int64_t count (void*);
int truth (void*);
int less (void*,void*);
void push (void*);
void push_bool (int);
void push_int (int64_t);
void push_dbl (double);
void push_flag (int);
void* pop ();
int64_t pop_int ();
double pop_dbl ();
void* top ();
void* under ();
uint32_t hash (void*);
vec_t* stack ();
vec_t* caller_stack ();
map_t* global ();
map_t* scope_reading ();
map_t* scope_writing ();
map_t* caller_scope ();
int depth ();
void stacktrace ();
code_t* compile (int);
code_t* hindsight (int);

extern arena_t *heap;
extern arena_t *ints;
extern arena_t *dbls;
extern arena_t *strs;
extern arena_t *vecs;
extern arena_t *maps;

extern int int_count;
extern int dbl_count;
extern int str_count;
extern int vec_count;
extern int map_count;

extern int heap_mem;
extern int ints_mem;
extern int dbls_mem;
extern int strs_mem;
extern int vecs_mem;
extern int maps_mem;

extern map_t *core;
extern map_t *super_str;
extern map_t *super_vec;
extern map_t *super_map;
extern vec_t *stacks;
extern vec_t *scopes;
extern int *calls;
extern int call_count;
extern int call_limit;
extern int ip;
extern int flags;

extern code_t *code;
extern int code_count;
extern int code_limit;
extern FILE *stream_output;
