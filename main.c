#include "common.h"
#include "vectors.h"
#include "machine.h"

// from parser.c can't be bother to make a whole new file for this
bool parse_and_gen_bytecode(Env* env, char* src);

char* load_file(char* file_name) {
  FILE* fptr = fopen(file_name, "r");
  if(fptr == NULL) {
    fprintf(stderr, "ERROR: %s: %s\n", file_name, strerror(errno));
    exit(1);
  }

  fseek(fptr, 0, SEEK_END);
  i32 size = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);

  char* dst_buf = ALLOCATE(char, size +1);
  fread(dst_buf, 1, size, fptr);
  dst_buf[size] = '\0';
  fclose(fptr);
  return dst_buf;
}

i32 main(i32 argc, char** argv) {
  char* src = NULL;
  if(argc == 2) {
    src = load_file(argv[1]);
  }
  else {
    fprintf(stderr, "Usage: ./play src-file\n");
    return 1;
  }

  Env env;
  env_allocate(&env);

  bool ok = parse_and_gen_bytecode(&env, src);
  if(ok) {
    env_print_instructions(&env);
    bool iok = interpret(&env);
    if(!iok) {
      fprintf(stderr, "Interpeter Error.. Aborting\n");
    }
  }
  env_deallocate(&env);
  free(src);
  return 0;
}
