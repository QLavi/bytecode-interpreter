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
  [Op_Jump_If_False] = "JUMP_IF_FALSE",
  [Op_Jump] = "JUMP",
  [Op_Loop] = "LOOP",
  [Op_Build_List] = "BUILD_LIST",
  [Op_List_Subscript] = "LIST_SUBSCRIPT"
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

void print_value(value data) {
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

static i32 opcode_byte2(byte inst, value data, i32 idx, i32 offset) {
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

static i32 opcode_byte1(byte inst, i32 offset) {
  printf("%04i  %-20s\n", offset, opc_to_str[inst]);
  return offset +1;
}

static i32 opcode_jump(byte inst, i32 idx, i32 sign, i32 offset) {
  printf("%04i  %-20s Jmp: %i\n", offset, opc_to_str[inst], offset + sign * idx +3);
  return offset +3;
}
static i32 opcode_byte3(byte inst, i32 idx, i32 offset) {
  printf("%04i  %-20s %i\n", offset, opc_to_str[inst], idx);
  return offset +3;
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
      case Op_List_Subscript:
      case Op_Return: {
        offset = opcode_byte1(inst, offset);
      } break;
      case Op_Define_Global:
      case Op_Set_Global:
      case Op_Get_Global:
      case Op_Set_Local:
      case Op_Get_Local:
      case Op_Push_Constant: {
        idx = env->stream.data[offset +1];
        data = env->constants.data[idx];
        offset = opcode_byte2(inst, data, idx, offset);
      } break;
      case Op_Loop:
      case Op_Jump:
      case Op_Jump_If_False: {
        byte low = env->stream.data[offset +1];
        byte high = env->stream.data[offset +2];
        i32 idx = (low << 8) | high;
        offset = opcode_jump(inst, idx, inst == Op_Loop ? -1 : 1, offset);
      } break;
      case Op_Build_List: {
        byte low = env->stream.data[offset +1];
        byte high = env->stream.data[offset +2];
        i32 idx = (low << 8) | high;
        offset = opcode_byte3(inst, idx, offset);
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

void build_list(Env* env, i32 elem_count) {
  i32 start_idx = env->eval_stack.count - elem_count;
  i32 end_idx = start_idx + elem_count;
  Object_List* list = allocate_list(env);
  for(i32 x = start_idx; x < end_idx; x+=1) {
    value_vector_pushback(&list->vector, eval_peek(env, (elem_count -1) - x));
  }
  for(i32 x = 0; x < elem_count; x+=1) {
    eval_pop(env);
  }
  eval_push(env, Value_Object(list));
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
      case Op_Build_List: {
        i32 elem_count = (ip[idx +1] << 8) | ip[idx +2];
        build_list(env, elem_count);
        idx += 3;
      } break;
      case Op_List_Subscript: {
        if(!Object_isList(eval_peek(env, 1)) && !Value_isNumber(eval_peek(env, 0))) {
          runtime_error(env, "%s: Either Object is not subscriptable or Index is not a number", opc_to_str[inst], eval_peek(env, 0));
          return false;
        }
        value index = eval_pop(env);
        value list = eval_pop(env);
        value elem = Object_asList(list)->vector.data[(i32)(index.number)];
        eval_push(env, elem);
        idx += 1;
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
        uint8_t slot = ip[idx +1];
        eval_push(env, env->eval_stack.data[slot]);
        idx += 2;
      } break;
      case Op_Set_Local: {
        uint8_t slot = ip[idx +1];
        env->eval_stack.data[slot] = eval_peek(env, 0);
        idx += 2;
      } break;
      case Op_Jump_If_False: {
        i32 offset = (ip[idx +1] << 8) | ip[idx +2];
        idx += 3;
        if(is_falsey(eval_peek(env, 0)))
          idx += offset;
      } break;
      case Op_Jump: {
        i32 offset = (ip[idx +1] << 8) | ip[idx +2];
        idx += 3;
        idx += offset;
      } break;
      case Op_Loop: {
        i32 offset = (ip[idx +1] << 8) | ip[idx +2];
        idx += 3;
        idx -= offset;
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
