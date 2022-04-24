#pragma once
#include "vectors.h"
#include "table.h"

enum {
  Op_Error,         // placeholder to tell wrong opcode was present
  Op_Push_Constant, // 2 bytes
  Op_Add,           // 1 byte
  Op_Sub,           // " 
  Op_Mul,           // " 
  Op_Div,           // " 
  Op_Neg,           // " 
  Op_Not,           // !
  Op_True,          // true
  Op_False,         // false
  Op_Less,          // <
  Op_Greater,       // >
  Op_Equal,         // ==
  Op_Null,          // null
  Op_Return,        // "
  Op_Print,
  Op_Pop,
  Op_Define_Global,
  Op_Set_Global,
  Op_Get_Global,
  Op_Set_Local,
  Op_Get_Local,
  Op_Jump_If_False,
  Op_Jump,
  Op_Loop,
  Op_Build_List,
  Op_List_Subscript,
};
typedef struct Env Env;
struct Env {
  byte_vector stream;
  value_vector constants;
  value_vector eval_stack;
  byte* ip;
  Object* objects;
  Table interned_strings;
  Table globals;
};

void env_allocate(Env* env);
void env_print_instructions(Env* env);
bool interpret(Env* env);
void env_deallocate(Env* env);
void print_value(value data);
