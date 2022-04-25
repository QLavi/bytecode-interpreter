#pragma once
#include "value.h"
#include "vectors.h"

typedef enum {
  Ok_String,
  Ok_List,
  Ok_Function,
} Object_Kind;

struct Object {
  Object_Kind kind;
  struct Object* next;
};
#define Get_Object_Kind(val)    (Value_asObject(val)->kind)

struct Object_List {
  Object object;
  value_vector vector;
};
#define Object_asList(val)    ((Object_List*)Value_asObject(val))
#define Object_isList(val)    (object_istype(val, Ok_List))

struct Object_String {
  Object object;
  char* str;
  int len;
  uint32_t hash;
};
#define Object_isString(val)    (object_istype(val, Ok_String))
#define Object_asString(val)    ((Object_String*)Value_asObject(val))
#define Get_Object_CString(val) (((Object_String*)Value_asObject(val))->str)

typedef struct {
  Object object;
  byte_vector code;
  i32 arity;
  Object_String* name;
} Object_Function;

#define Object_isFunction(val)  (object_istype(val, Ok_Function))
#define Object_asFunction(val)  ((Object_Function*)Value_asObject(val))

static inline bool object_istype(value val, Object_Kind kind) {
  return Value_isObject(val) && Value_asObject(val)->kind == kind;
}

typedef struct Env Env;
Object_String* allocate_string(Env* env, char* str, int len, uint32_t hash);
Object_String* take_string(Env* env, char* str, int len);
void free_objects(Env* env);
Object_String* object_string_cpy(Env* env, char* chars, int len);
Object_List* allocate_list(Env* env);
void print_object(value val);
Object_Function* make_function(Env* env);
