#pragma once
#include "stdlib.h"
#include "string.h"

void* x_alloc(void* old_ptr, size_t elem_size, int count, const char* file, int line);
#define ALLOCATE(type, count) (type*)x_alloc(NULL, sizeof(type), count, __FILE__, __LINE__)
#define REALLOCATE(type, old_ptr, count) (type*)x_alloc(old_ptr, sizeof(type), count, __FILE__, __LINE__)
#define FREE(old_ptr) x_alloc(old_ptr, 0, 0, __FILE__, __LINE__)