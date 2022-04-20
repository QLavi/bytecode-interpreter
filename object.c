#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "value.h"
#include "machine.h"
#include "table.h"

Object* allocate_object(Env* env, size_t size, Object_Kind kind) {
  Object* ob = ALLOCATE(Object, size);
  ob->kind = kind;
  ob->next = env->objects;
  env->objects = ob;
  return ob;
}

void free_object(Object* object) {
  switch(object->kind) {
    case Ok_String: {
      Object_String* str = (Object_String*)object;
      FREE(str->str);
      FREE(object);
    } break;
  }
}

void free_objects(Env* env) {
  Object* ob = env->objects;
  while(ob != NULL) {
    Object* next = ob->next;
    free_object(ob);
    ob = next;
  }
}

Object_String* allocate_string(Env* env, char* str, int len, uint32_t hash) {
  Object_String* interned = table_find_string(&env->interned_strings, str, len,
    hash);
  if(interned != NULL) {
    // don't care about the string if it already there..
    free(str);
    return interned;
  }
  
  Object_String* string = (Object_String*)allocate_object(env, 
    sizeof(Object_String),
    Ok_String);
  string->str = str;
  string->len = len;
  string->hash = hash;
  table_set(&env->interned_strings, string, Value_Null());
  return string;
}

Object_String* take_string(Env* env, char* str, int len) {
  uint32_t hash = fnv_1a(str, len);
  return allocate_string(env, str, len, hash);
}

Object_String* object_string_cpy(Env* env, char* str, int len) {
  uint32_t hash = fnv_1a(str, len);

  char* heap_str = ALLOCATE(char, len+1);
  memcpy(heap_str, str, len);
  heap_str[len] = '\0';
  return allocate_string(env, heap_str, len, hash);
}

void print_object(value val) {
  switch(Get_Object_Kind(val)) {
    case Ok_String:
      printf("%s", Get_Object_CString(val));
      break;
  }
}
