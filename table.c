#include "table.h"

// I will try to make this as understandable as possible.
// I struggle with open_addressing a lot

// number of entries / number of buckets
#define Table_Load_Factor 0.75

void table_allocate(Table* table) {
  table->entries = ALLOCATE(Entry, 8);
  table->count = 0;
  table->cap = 8;

  for(int x = 0; x < table->cap; x+=1) {
    table->entries[x].key = NULL;
    table->entries[x].val = Value_Null();
  }
}
void table_deallocate(Table* table) {
  FREE(table->entries);
  table->count = 0;
  table->cap = 0;
}

uint32_t fnv_1a(char* bytes, int len) {
  uint32_t hash = 2166136261;
  for(int x = 0; x < len; x+=1) {
    hash ^= (uint8_t)bytes[x];
    hash *= 16777619;
  }
  return hash;
}

Object_String* table_find_string(Table* table, char* str, int len, uint32_t hash) {
  if(table->count == 0) return NULL;
  uint32_t idx = hash % table->cap;
  for(;;) {
    Entry* entry = &table->entries[idx];

    if(entry->key == NULL) {
      // check if its tombstone. if it isn't then no such entry exists
      if(Value_isNull(entry->val))
        return NULL;
    }
    // memcmp is at the last because its slowest part.. also the last test
    // early bail out if hash/length are not the same
    // memcmp's failure means hash collision occured
    else if(entry->key->len == len &&
      entry->key->hash == hash &&
      memcmp(entry->key->str, str, len) == 0)
      return entry->key;

    idx = (idx +1) % table->cap;
  }
}

Entry* table_find_entry(Entry* entries, int cap, Object_String* key) {
  // wraps the idx around table->caps
  uint32_t idx = key->hash % cap;
  Entry* tombstone = NULL;

  // this infinite for loop assumes that will stop after finding a empty entry
  // or actual entry
  // and this is true because table is never full..
  for(;;) {
    Entry* entry = &entries[idx];

    // checking for tombstone
    if(entry->key == NULL) {
      // empty bucket. either return old tombstone if present or this new found
      // empty entry
      if(Value_isNull(entry->val))
        return tombstone != NULL ? tombstone : entry;
      else
        // tombstone found keep that in check for later use
        if(tombstone == NULL)
          tombstone = entry;
    }
    // return the key as normal
    else if(entry->key == key)
      return entry;

    // collision occured do linear probing
    idx = (idx +1) % cap;
  }
}

void adjust_table_cap(Table* table, int new_cap) {
  Entry* entries = ALLOCATE(Entry, new_cap);

  // initialize the table with null values
  for(int x = 0; x < new_cap; x+=1) {
    entries[x].key = NULL;
    entries[x].val = Value_Null();
  }

  // need to rehash the complete table again because of hash % table->cap
  // will change.
  // so try finding bucket for old keys and assign it new buckets of the new
  // entries
  table->count = 0;
  for(int x = 0; x < table->cap; x+=1) {
    Entry* entry = &table->entries[x];

    // don't check for tombstones.. they will be discarded..
    if(entry->key == NULL) continue;

    Entry* dst = table_find_entry(entries, new_cap, entry->key);
    dst->key = entry->key;
    dst->val = entry->val;
    table->count += 1;
  }

  FREE(table->entries);
  table->entries = entries;
  table->cap = new_cap;
}

bool table_set(Table* table, Object_String* key, value val) {

  // if table cap can't accomodate the entries allocate more!
  if(table->count +1 > table->cap * Table_Load_Factor) {
    int new_cap = table->cap * 2;
    adjust_table_cap(table, new_cap);
  }
  // check if it already there or not
  Entry* entry = table_find_entry(table->entries, table->cap, key); 
  bool is_new_key = entry->key == NULL;

  // so it new one.. increase the count (tombstone are not counted)
  if(is_new_key && Value_isNull(entry->val))
    table->count +=1;

  entry->key = key;
  entry->val = val;

  // it returns true if its a new one is inserted
  return is_new_key;
}

bool table_get(Table* table, Object_String* key, value* val) {
  // Duh!
  if(table->count == 0) return false;

  // pretty much self explanatory
  Entry* entry = table_find_entry(table->entries, table->cap, key);
  if(entry->key == NULL) return false;

  *val = entry->val;
  return true;
}

bool table_delete(Table* table, Object_String* key) {
  if(table->count == 0) return false;

  Entry* entry = table_find_entry(table->entries, table->cap, key);
  if(entry->key == NULL) return false;

  // set a sentinel value to represent Tombstone
  // tombstone is entry that is grave of deleted entry
  // it is placed so that probe sequence don't break if deleted entry
  // was part of them
  entry->key = NULL;
  entry->val = Value_Bool(true);
  return true;
}
