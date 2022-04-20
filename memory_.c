#include <stdio.h>
#include "memory_.h"

void* x_alloc(void* old_ptr, size_t elem_size, int count,
  const char* file, int line) {
  (void)line;
  (void)file;
  if(old_ptr != NULL && count == 0) {
    // printf("%-10s:%i: Free_Request: [%p]\n", file, line, old_ptr);
    free(old_ptr);
    return NULL;
  }
  if(count != 0) {
    // if(old_ptr == NULL)
    //   printf("%s:%i: Alloc_Request: [%zu * %i]\n", file, line, elem_size,
    //     count);
    // else
    //   printf("%s:%i: Realloc_Request: [%zu * %i]\n", file, line, elem_size,
    //     count);

    void* new_ptr = realloc(old_ptr, elem_size * count);
    if(new_ptr == NULL) {
      fprintf(stderr, "Out of Memory.. Aborting\n");
      exit(1);
    }
    return new_ptr;
  }
  return NULL; // just here to silent -Wreturn-type
}