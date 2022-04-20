#include <stdarg.h>
#include "object.h"
#include "machine.h"

static char* opc_to_str[] = {
  [Op_Push_Constant] = "PUSH_CONSTANT",
  [Op_Return] = "RETURN",
  [Op_Add] = "ADD",
  [Op_Sub] = "SUB",
  [Op_Mul] = "MUL",
  [Op_Div] = "DIV",
  [Op_Not] = "NOT",
  [Op_Equal] = "CHECK_EQUAL",
  [Op_Less] = "CHECK_LESS",
  [Op_Greater] = "CHECK_GREATER",
  [Op_True] = "PUSH_TRUE", 
  [Op_False] = "PUSH_FALSE",
  [Op_Null] = "PUSH_NULL",
  [Op_Neg] = "NEGATE",
  [Op_Print] = "PRINT",
  [Op_Pop] = "POP",
  [Op_Define_Global] = "DEFINE_GLOBAL",
  [Op_Set_Global] = "SET_GLOBAL",
  [Op_Get_Global] = "GET_GLOBAL",
  [Op_Set_Local] = "SET_LOCAL",
  [Op_Get_Local] = "GET_LOCAL",
};

void env_allocate(Env* env) {
  byte_vector_allocate(&env->stream);
  value_vector_allocate(&env->constants);
  value_vector_allocate(&env->eval_stack);
  table_allocate(&env->interned_strings);
  table_allocate(&env->globals);
  env->objects = NULL;
}

void env_deallocate(Env* env) {
  byte_vector_deallocate(&env->stream);
  value_vector_deallocate(&env->constants);
  value_vector_deallocate(&env->eval_stack);
  table_deallocate(&env->interned_strings);
  table_deallocate(&env->globals);
  free_objects(env);
}

void env_push_opcode(Env* env, byte opc) {
  byte_vector_pushback(&env->stream, opc);
}

i32 env_push_constant(Env* env, value val) {
  value_vector_pushback(&env->constants, val);
  return env->constants.count -1;
}

static void eval_push(Env* env, value val) {
  value_vector_pushback(&env->eval_stack, val);
}
static value eval_pop(Env* env) {
  return value_vector_pop(&env->eval_stack);
}

static value eval_peek(Env* env, i32 offset) {
  i32 count = env->eval_stack.count;
  return env->eval_stack.data[(count -1) - offset];
}
static void eval_reset_stack(Env* env) {
  env->eval_stack.count = 0;
}

static void print_value(value data) {
  switch(data.kind) {
    case Vk_Bool:
      printf("%s", Value_asBool(data) ? "true" : "false");
      break;
    case Vk_Number:
      printf("%g", Value_asNumber(data));
      break;
    case Vk_Null:
      printf("null");
      break;
    case Vk_Object:
      print_object(data);
      break;
    case Vk_Error:
      printf("Object is uninitialized. or clobbered");
      break;
    default: return;
  }
}

static i32 opc_2byte(byte inst, value data, i32 idx, i32 offset) {
  printf("%04i  %-20s %i ", offset, opc_to_str[inst], idx);
  if(inst == Op_Define_Global || inst == Op_Get_Global) {
    printf("(");
    print_value(data);
    printf(")");
  }
  else if (data.kind != Vk_Number) {
    printf("(\"");
    print_value(data);
    printf("\")");
  }
  else {
    printf("(");
    print_value(data);
    printf(")");
  }
  putc('\n', stdout);
  return offset +2;
}

static i32 opc_1byte(byte inst, i32 offset) {
  printf("%04i  %-20s\n", offset, opc_to_str[inst]);
  return offset +1;
}

static void runtime_error(Env* env, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fputs("Runtime Error: ", stderr);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputs(".\n", stderr);
  eval_reset_stack(env);
}

void env_print_instructions(Env* env) {
  i32 offset = 0;
  i32 idx = 0;
  value data;
  byte inst = 0;
  printf("=== Disassembled Bytecode ===\n");
  while(offset < env->stream.count) {
    inst = env->stream.data[offset];

    switch(inst) {
      case Op_Add:
      case Op_Sub:
      case Op_Mul:
      case Op_Div:
      case Op_Neg:
      case Op_True:
      case Op_False:
      case Op_Null:
      case Op_Not:
      case Op_Equal:
      case Op_Less:
      case Op_Greater:
      case Op_Print:
      case Op_Pop:
      case Op_Return: {
        offset = opc_1byte(inst, offset);
      } break;
      case Op_Define_Global:
      case Op_Set_Global:
      case Op_Get_Global:
      case Op_Set_Local:
      case Op_Get_Local:
      case Op_Push_Constant: {
        idx = env->stream.data[offset +1];
        data = env->constants.data[idx];
        offset = opc_2byte(inst, data, idx, offset);
      } break;
      default:
        fprintf(stderr, "Invalid opcode found\n");
        return;
    }
  }
  printf("=== End ===\n\n");
}

bool is_falsey(value val) {
  return Value_isNull(val) || (Value_isBool(val) && !Value_asBool(val));
}

bool check_equality(value x, value y) {
  if (x.kind != y.kind) return false;
  switch(x.kind) {
    case Vk_Bool: return Value_asBool(x) == Value_asBool(y);
    case Vk_Null: return true;
    case Vk_Number: return Value_asNumber(x) == Value_asNumber(y);

    // when strings is are parsed and stored
    // machine's internal table is checked if already same string is present
    // if that's true then return the same string
    // so comparing pointer is used here to check equality
    case Vk_Object: return Value_asObject(x) == Value_asObject(y);
    default: return false;
  }
}

void concatenate_strings(Env* env) {
  Object_String* y = Object_asString(eval_pop(env));
  Object_String* x = Object_asString(eval_pop(env));

  int len = x->len + y->len;
  char* concat = ALLOCATE(char, len +1);
  memcpy(concat, x->str, x->len);
  memcpy(concat + x->len, y->str, y->len);
  concat[len] = '\0';

  Object_String* str = take_string(env, concat, len);
  eval_push(env, Value_Object(str));
}

bool interpret(Env* env) { 
  byte inst;
  i32 i_count = env->stream.count;
  i32 idx = 0;
  byte* ip = env->stream.data;

  while(idx < i_count) {
    inst = ip[idx];
    switch(inst) {
      case Op_Add: {
        if(Object_isString(eval_peek(env, 0))
          || Object_isString(eval_peek(env, 1))) {
          concatenate_strings(env);
        }
        else if(Value_isNumber(eval_peek(env, 0)) 
          || Value_isNumber(eval_peek(env, 1))) {
          value y = eval_pop(env);
          value x = eval_pop(env);
          double r = Value_asNumber(x) + Value_asNumber(y);
          eval_push(env, Value_Number(r));
        }
        else {
          runtime_error(env, "Operands must be numbers");
          return false; 
        }
        idx += 1;
      } break;
      case Op_Sub: {
        if(!Value_isNumber(eval_peek(env, 0)) ||
          !Value_isNumber(eval_peek(env, 1))) {
          runtime_error(env, "%s: Operands must be numbers", opc_to_str[inst]);
          return false; 
        }
        value y = eval_pop(env);
        value x = eval_pop(env);
        double r = Value_asNumber(x) - Value_asNumber(y);
        eval_push(env, Value_Number(r));
        idx += 1;
      } break; 
      case Op_Mul: {
        if(!Value_isNumber(eval_peek(env, 0)) ||
          !Value_isNumber(eval_peek(env, 1))) {
          runtime_error(env, "%s: Operands must be numbers", opc_to_str[inst]);
          return false; 
        }
        value y = eval_pop(env);
        value x = eval_pop(env);
        double r = Value_asNumber(x) * Value_asNumber(y);
        eval_push(env, Value_Number(r));
        idx += 1;
      } break;
      case Op_Div: {
        if(!Value_isNumber(eval_peek(env, 0)) ||
          !Value_isNumber(eval_peek(env, 1))) {
          runtime_error(env, "%s: Operands must be numbers", opc_to_str[inst]);
          return false; 
        }
        value y = eval_pop(env);
        value x = eval_pop(env);
        double r = Value_asNumber(x) / Value_asNumber(y);
        eval_push(env, Value_Number(r));
        idx += 1;
      } break;
      case Op_Less: {
        if(!Value_isNumber(eval_peek(env, 0)) || 
          !Value_isNumber(eval_peek(env, 1))) {
          runtime_error(env, "Operands must be numbers");
          return false; 
        }
        value y = eval_pop(env);
        value x = eval_pop(env);
        double r = Value_asNumber(x) < Value_asNumber(y);
        eval_push(env, Value_Bool(r));
        idx += 1;
      } break;
      case Op_Greater: {
        if(!Value_isNumber(eval_peek(env, 0)) || 
          !Value_isNumber(eval_peek(env, 1))) {
          runtime_error(env, "Operands must be numbers");
          return false; 
        }
        value y = eval_pop(env);
        value x = eval_pop(env);
        double r = Value_asNumber(x) > Value_asNumber(y);
        eval_push(env, Value_Bool(r));
        idx += 1;
      } break;
      case Op_Equal: {
        value y = eval_pop(env);
        value x = eval_pop(env);
        eval_push(env, Value_Bool(check_equality(x, y)));
        idx += 1;
      } break;
      case Op_Neg: {
        if(!Value_isNumber(eval_peek(env, 0))) {
          runtime_error(env, "%s: Operands must be numbers", opc_to_str[inst]);
          return false;
        }
        value x = eval_pop(env);
        eval_push(env, Value_Number(-Value_asNumber(x)));
        idx += 1;
      } break;
      case Op_Not: {
        eval_push(env, Value_Bool(is_falsey(eval_pop(env))));
        idx += 1;
      } break;
      case Op_Push_Constant: {
        value val = env->constants.data[ip[idx +1]];
        eval_push(env, val);
        idx += 2;
      } break;
      case Op_Define_Global: {
        Object_String* name = Object_asString(env->constants.data[ip[idx +1]]);
        table_set(&env->globals, name, eval_peek(env, 0));
        eval_pop(env);
        idx += 2;
      } break;
      case Op_Get_Global: {
        Object_String* name = Object_asString(env->constants.data[ip[idx +1]]);
        value val;
        if(!table_get(&env->globals, name, &val)) {
          runtime_error(env, "Undefined variable '%s'", name->str);
          return false;
        }
        eval_push(env, val);
        idx += 2;
      } break;
      case Op_Set_Global: {
        Object_String* name = Object_asString(env->constants.data[ip[idx +1]]);

        // table_set returns false if key is not new. which means this key
        // never existed. so it is a runtime error because declaration is required
        // before variable can be assigned
        if(table_set(&env->globals, name, eval_peek(env, 0))) {
          table_delete(&env->globals, name);
          runtime_error(env, "Undefined variable '%s'", name->str);
          return false;
        }
        idx += 2;
      } break;
      case Op_Get_Local: {
        uint8_t slot = ip[idx + 1];
        eval_push(env, env->eval_stack.data[slot]);
        idx += 2;
      } break;
      case Op_Set_Local: {
        uint8_t slot = ip[idx + 1];
        env->eval_stack.data[slot] = eval_peek(env, 0);
        idx += 2;
      } break;
      case Op_True: {
        eval_push(env, Value_Bool(true));
        idx += 1;
      } break;
      case Op_False: {
        eval_push(env, Value_Bool(false));
        idx += 1;
      } break;
      case Op_Null: {
        eval_push(env, Value_Null());
        idx += 1;
      } break;
      case Op_Pop: {
        eval_pop(env);
        idx += 1;
      } break;
      case Op_Print: {
        print_value(eval_pop(env));
        putc('\n', stdout);
        idx += 1;
      } break;
      case Op_Return: return true;
      default:
        fprintf(stderr, "Invalid Instruction Given\n");
        return false;
    }
  }
  return false; // just here to silent -Wreturn-type
}
