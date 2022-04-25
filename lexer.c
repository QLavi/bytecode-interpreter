#include <ctype.h>
#include "lexer.h"
Lexer lexer;

void set_lexer_state(char* src) {
  lexer.begin = lexer.current = src;
}
static char advance(void) {
  lexer.current += 1;
  return lexer.current[-1];
}

static Token error_token(char* descr) {
  return (Token) {
    .kind = Tk_Error,
    .str = descr,
    .len = strlen(descr)
  };
}

static void ignore_whitespace(void) {
  for(;;) {
    while(isspace(lexer.current[0]))
      advance();

    if(lexer.current[0] == '#')
      while(lexer.current[0] != '\n')
        advance();
    else
      return;
  }
}

static Token make_token(i32 kind) {
  return (Token) {
    .kind = kind,
    .str = lexer.begin,
    .len = lexer.current - lexer.begin
  };
}

static Token string() {
  char c = lexer.current[0];

  while(c != '"') {
    if(c == '\n')
      return error_token("Unterminated String");
    else
      c = advance();
  }
  return make_token(Tk_String);
}

i32 check_keyword(i32 idx, i32 len, char* rest, i32 kind) {
  if(!strncmp(lexer.begin +idx, rest, len))
    return kind;
  else
    return Tk_Identifier;
}

static i32 identifier_or_keyword_kind() {
  switch(lexer.begin[0]) {
    case 'a': return check_keyword(1, 2, "nd", Tk_And);
    case 'o': return check_keyword(1, 1, "r", Tk_Or); 
    case 'i': return check_keyword(1, 1, "f", Tk_If);
    case 'e': return check_keyword(1, 3, "lse", Tk_Else);
    case 'w': return check_keyword(1, 4, "hile", Tk_While);
    case 't': return check_keyword(1, 3, "rue", Tk_True);
    case 'n': return check_keyword(1, 3, "ull", Tk_Null);
    case 'r': return check_keyword(1, 5, "eturn", Tk_Return);
    case 'l': return check_keyword(1, 2, "et", Tk_Let);
    case 'f': {
      if(lexer.current - lexer.begin > 1) {
        switch(lexer.begin[1]) {
          case 'a': return check_keyword(2, 3, "lse", Tk_False);
          case 'o': return check_keyword(2, 1, "r", Tk_For);
        }
      }
    } break;
    case 'p': {
      if(lexer.current - lexer.begin > 1) {
        switch(lexer.begin[1]) {
          case 'r': {
            if(lexer.current - lexer.begin > 1) {
              switch(lexer.begin[2]) {
                case 'i': return check_keyword(3, 2, "nt", Tk_Print);
                case 'o': return check_keyword(3, 1, "c", Tk_Proc);
              }
            }
          } break;
        }
      }
    } break;
  }
  return Tk_Identifier;
}

static Token identifier_or_keyword() {
  while(isalpha(lexer.current[0]) || isdigit(lexer.current[0]) || 
    lexer.current[0] == '_')
    advance();

  return make_token(identifier_or_keyword_kind());
}

static Token number() {
  while(isdigit(lexer.current[0]))
    advance();
  if(lexer.current[0] == '.')
    advance();
  while(isdigit(lexer.current[0]))
    advance();
  return make_token(Tk_Number);
}

static bool match(char c) {
  if(lexer.current[0] == '\0') return false;
  else if(lexer.current[0] != c) return false;
  advance();
  return true;
}

Token next_token(void) {
  ignore_whitespace();

  lexer.begin = lexer.current;
  if(lexer.current[0] == '\0')
    return make_token(Tk_Eof);

  char c = advance();
  if(c == '"')
    return string();
  if(isalpha(c))
    return identifier_or_keyword();
  if(isdigit(c))
    return number();

  switch(c) {
    case '+': return make_token(match('=') ? Tk_Plus_Equal : Tk_Plus);
    case '-': return make_token(match('=') ? Tk_Minus_Equal : Tk_Minus);
    case '*': return make_token(match('=') ? Tk_Star_Equal : Tk_Star);
    case '/': return make_token(match('=') ? Tk_Slash_Equal : Tk_Slash);
    case '=': return make_token(match('=') ? Tk_Equal_Equal : Tk_Equal);
    case '<': return make_token(match('=') ? Tk_Less_Equal : Tk_Less);
    case '>': return make_token(match('=') ? Tk_Greater_Equal : Tk_Greater);
    case '!': return make_token(match('=') ? Tk_Bang_Equal : Tk_Bang);
    case '[': return make_token(Tk_Left_SqrParen);
    case ']': return make_token(Tk_Right_SqrParen);
    case '(': return make_token(Tk_Left_Paren);
    case ')': return make_token(Tk_Right_Paren);
    case '{': return make_token(Tk_Left_Brace);
    case '}': return make_token(Tk_Right_Brace);
    case ',': return make_token(Tk_Comma);
    case ';': return make_token(Tk_Semicolon);
  }
  return error_token("Unexpected Character");
}

