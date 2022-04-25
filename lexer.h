#pragma once
#include "common.h"

enum {
  Tk_Error, Tk_Eof,
  Tk_Right_Brace, Tk_Left_Brace, Tk_Left_Paren, Tk_Right_Paren,
  Tk_Left_SqrParen, Tk_Right_SqrParen,
  Tk_Comma, Tk_Semicolon,

  Tk_Plus, Tk_Minus, Tk_Star, Tk_Slash,
  Tk_Plus_Equal, Tk_Minus_Equal, Tk_Star_Equal, Tk_Slash_Equal,
  Tk_Bang, Tk_Bang_Equal, Tk_Equal, Tk_Equal_Equal,
  Tk_Less, Tk_Less_Equal, Tk_Greater, Tk_Greater_Equal,
  Tk_And, Tk_Or,

  Tk_Identifier, Tk_String, Tk_Number,
  Tk_True, Tk_False,

  Tk_For, Tk_While, Tk_If, Tk_Else,
  Tk_Print, Tk_Proc, Tk_Return, Tk_Let, Tk_Null
};

typedef struct {
  i32 kind;
  char* str;
  i32 len;
} Token;

typedef struct {
  char* begin;
  char* current;
} Lexer;

void set_lexer_state(char* src);
Token next_token(void);

