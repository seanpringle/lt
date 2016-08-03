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

void op_nop ();
void op_print ();
void op_coroutine ();
void op_resume ();
void op_yield ();
void op_call ();
void op_call_lit ();
void op_return ();
void op_lit ();
void op_scope ();
void op_smudge ();
void op_litstack ();
void op_unscope ();
void op_litscope ();
void op_mark ();
void op_limit ();
void op_string ();
void op_array ();
void op_table ();
void op_global ();
void op_local ();
void op_defnil ();
void op_nil ();
void op_self ();
void op_self_push ();
void op_self_drop ();
void op_shunt ();
void op_true ();
void op_false ();
void op_drop ();
void op_drop_all ();
void op_test ();
void op_jmp ();
void op_jfalse ();
void op_jtrue ();
void op_define ();
void op_for ();
void op_keys ();
void op_values ();
void op_assign ();
void op_assign_lit ();
void op_find ();
void op_find_lit ();
void op_inherit ();
void op_set ();
void op_get ();
void op_get_lit ();
void op_add ();
void op_add_lit ();
void op_neg ();
void op_sub ();
void op_mul ();
void op_div ();
void op_mod ();
void op_eq ();
void op_lt ();
void op_lt_lit ();
void op_gt ();
void op_lte ();
void op_gte ();
void op_not ();
void op_concat ();
void op_count ();
void op_match ();
void op_status ();

enum {
  OP_NOP=1,
  OP_PRINT,
  OP_COROUTINE,
  OP_RESUME,
  OP_YIELD,
  OP_CALL,
  OP_CALL_LIT,
  OP_CALL_OTHER,
  OP_RETURN,
  OP_STRING,
  OP_ARRAY,
  OP_TABLE,
  OP_GLOBAL,
  OP_LOCAL,
  OP_SCOPE,
  OP_SMUDGE,
  OP_LITSTACK,
  OP_UNSCOPE,
  OP_LITSCOPE,
  OP_MARK,
  OP_LIMIT,
  OP_TEST,
  OP_JMP,
  OP_JZ,
  OP_JNZ,
  OP_NEXT,
  OP_FOR,
  OP_KEYS,
  OP_VALUES,
  OP_NIL,
  OP_SELF,
  OP_SELF_PUSH,
  OP_SELF_DROP,
  OP_SHUNT,
  OP_TRUE,
  OP_FALSE,
  OP_DEFNIL,
  OP_LIT,
  OP_DEFINE,
  OP_ASSIGN,
  OP_ASSIGN_LIT,
  OP_FIND,
  OP_FIND_LIT,
  OP_SET,
  OP_INHERIT,
  OP_GET,
  OP_GET_LIT,
  OP_DROP,
  OP_DROP_ALL,
  OP_ADD,
  OP_ADD_LIT,
  OP_NEG,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_NOT,
  OP_EQ,
  OP_LT,
  OP_LT_LIT,
  OP_GT,
  OP_LTE,
  OP_GTE,
  OP_CONCAT,
  OP_COUNT,
  OP_MATCH,
  OP_STATUS,
};
