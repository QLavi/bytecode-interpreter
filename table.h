#pragma once
#include "common.h"
#include "object.h"

typedef struct {
  Object_String* key;
  value val;
} Entry;

typedef struct {
  Entry* entries;
  int count, cap;
} Table;

void table_allocate(Table* table);
void table_deallocate(Table* table);
bool table_get(Table* table, Object_String* key, value* val);
Object_String* table_find_string(Table* table, char* str, int len, uint32_t hash);
bool table_set(Table* table, Object_String* key, value val);
bool table_delete(Table* table, Object_String* key);
uint32_t fnv_1a(char* bytes, int len);
