#include "lexer.h"
#include "machine.h"
#include "object.h"

typedef struct {
  Token previous, current;
  bool had_error, panic_mode;
} Parser;
Parser parser;

typedef struct {
  Token name;
  int active_on;
} Local;

typedef struct {
  Local locals[16];
  int count;
  int scope_depth;
} Locals_Info;

Locals_Info locals_info;

// Token consumption and error reporting
static void error_at(Token* token, char* descr) {
  if(parser.panic_mode) return;
  parser.panic_mode = true;

  fprintf(stderr, "Error ");
  if(token->kind == Tk_Eof)
    fprintf(stderr, "at end, ");
  else if(token->kind != Tk_Error) {
    fprintf(stderr, "'%.*s': ", token->len, token->str);
  }
  fprintf(stderr, "%s.\n", descr);

  parser.had_error = true;
}

static void error_at_current(char* descr) {
  error_at(&parser.current, descr);
}
static void error(char* descr) {
  error_at(&parser.previous, descr);
}

static void advance_token(void) {
  parser.previous = parser.current;

  for(;;) {
    parser.current = next_token();

    if(parser.current.kind != Tk_Error) break;
    error_at_current(parser.current.str);
  }
}

static void parser_sync(void) {
  parser.panic_mode = false;

  while(parser.current.kind != Tk_Eof) {
    if(parser.previous.kind == Tk_Semicolon) return;
    switch(parser.current.kind) {
      case Tk_Def:
      case Tk_Let:
      case Tk_For:
      case Tk_While:
      case Tk_If:
      case Tk_Print:
      case Tk_Return:
        return;
      default: ;// do nothing
    }
    advance_token();
  }
}

static bool check_token(i32 token_kind) {
  return parser.current.kind == token_kind;
}

static bool match_token(i32 token_kind) {
  if(!check_token(token_kind))
    return false;
  advance_token();
  return true;
}

static void consume_token(i32 match_kind, char* descr) {
  if(parser.current.kind == match_kind)
    advance_token();
  else
    error_at_current(descr);
}

// Bytecode emitting routines
static void emit_byte(Env* env, byte b) {
  env_push_opcode(env, b);
}
static void emit_opcode_with_operand(Env* env, byte opc, byte opr) {
  env_push_opcode(env, opc);
  env_push_opcode(env, opr);
}
static void emit_constant(Env* env, value val) {
  i32 idx = env_push_constant(env, val);
  if(idx > UINT8_MAX)
    error("Constant count > max constants count.. not allowed");
  emit_opcode_with_operand(env, Op_Push_Constant, idx); 
}
static uint8_t identifier_constant(Env* env, Token* name) {
  return env_push_constant(env, Value_Object(object_string_cpy(env, name->str,
    name->len)));
}

// Parsing routines
enum {
  Prec_None,
  Prec_Assign,
  Prec_Or,
  Prec_And,
  Prec_Equality,
  Prec_Comparison,
  Prec_Term,
  Prec_Factor,
  Prec_Unary,
  Prec_Call,
  Prec_Primary,
};
typedef void (*Parse_Fn)(Env*, bool);
typedef struct {
  Parse_Fn prefix;
  Parse_Fn mixfix;
  i32 rbp;
} Parse_Rule;

static void parse_expr(Env* env, i32 lbp);

static void parse_group(Env* env, bool assignable) {
  (void)assignable;
  parse_expr(env, Prec_None);
  consume_token(Tk_Right_Paren, "Incomplete Set of () seen");
}

static void parse_number(Env* env, bool assignable) {
  (void)assignable;
  double val = strtod(parser.previous.str, NULL);
  emit_constant(env, Value_Number(val));
}

static void parse_unary(Env* env, bool assignable) {
  (void)assignable;
  i32 op_kind = parser.previous.kind;
  parse_expr(env, Prec_Unary);

  switch(op_kind) {
    case Tk_Minus: emit_byte(env, Op_Neg); break;
    case Tk_Bang:  emit_byte(env, Op_Not); break;
    default: return;
  }
}

static void parse_literal(Env* env, bool assignable) {
  (void)assignable;
  switch(parser.previous.kind) {
    case Tk_True:   emit_byte(env, Op_True); break;
    case Tk_False:  emit_byte(env, Op_False); break;
    case Tk_Null:   emit_byte(env, Op_Null); break;
    default: return;
  }
}
static void parse_string(Env* env, bool assignable) {
  (void)assignable;
  emit_constant(env, Value_Object(object_string_cpy(env, parser.previous.str +1,
    parser.previous.len -2)));
}

static bool identifiers_equal(Token* a, Token* b) {
  if(a->len != b->len) return false;
  return memcmp(a->str, b->str, a->len) == 0;
}

static int resolve_local(Token* name) {
  for(int x = locals_info.count -1; x >= 0; x-=1) {
    Local* local = &locals_info.locals[x];
    if(identifiers_equal(name, &local->name)) {
      if(local->active_on == -1) {
        error("Cannot read variable from its own initializer");
      }
      return x;
    }
  }
  return -1;
}

static void parse_ident(Env* env, bool assignable) {
  uint8_t get_op, set_op;
  int idx = resolve_local(&parser.previous);
  if(idx != -1) {
    get_op = Op_Get_Local;
    set_op = Op_Set_Local;
  }
  else {
    idx = identifier_constant(env, &parser.previous);
    get_op = Op_Get_Global;
    set_op = Op_Set_Global;
  }

  if(match_token(Tk_Equal) && assignable) {
    parse_expr(env, Prec_Assign);
    emit_opcode_with_operand(env, set_op, (uint8_t)idx);
  }
  else if(match_token(Tk_Plus_Equal) && assignable) {
    emit_opcode_with_operand(env, get_op, (uint8_t)idx);
    parse_expr(env, Prec_Assign);
    emit_byte(env, Op_Add);
    emit_opcode_with_operand(env, set_op, (uint8_t)idx);
  }
  else if(match_token(Tk_Minus_Equal) && assignable) {
    emit_opcode_with_operand(env, get_op, (uint8_t)idx);
    parse_expr(env, Prec_Assign);
    emit_byte(env, Op_Sub);
    emit_opcode_with_operand(env, set_op, (uint8_t)idx);
  }
  else if(match_token(Tk_Star_Equal) && assignable) {
    emit_opcode_with_operand(env, get_op, (uint8_t)idx);
    parse_expr(env, Prec_Assign);
    emit_byte(env, Op_Mul);
    emit_opcode_with_operand(env, set_op, (uint8_t)idx);
  }
  else if(match_token(Tk_Slash_Equal) && assignable) {
    emit_opcode_with_operand(env, get_op, (uint8_t)idx);
    parse_expr(env, Prec_Assign);
    emit_byte(env, Op_Div);
    emit_opcode_with_operand(env, set_op, (uint8_t)idx);
  }
  else {
    emit_opcode_with_operand(env, get_op, (uint8_t)idx);
  }
}

static void parse_binary(Env*, bool);

Parse_Rule rules[] = {
  [Tk_Error] =        {NULL,          NULL,         Prec_None},
  [Tk_Eof] =          {NULL,          NULL,         Prec_None},
  [Tk_Right_Brace] =  {NULL,          NULL,         Prec_None},
  [Tk_Left_Brace] =   {NULL,          NULL,         Prec_None},
  [Tk_Left_Paren] =   {parse_group,   NULL,         Prec_None},
  [Tk_Right_Paren] =  {NULL,          NULL,         Prec_None},
  [Tk_Comma] =        {NULL,          NULL,         Prec_None},
  [Tk_Semicolon] =    {NULL,          NULL,         Prec_None},

  [Tk_Plus] =         {NULL,          parse_binary, Prec_Term},
  [Tk_Minus] =        {parse_unary,   parse_binary, Prec_Term},
  [Tk_Star] =         {NULL,          parse_binary, Prec_Factor},
  [Tk_Slash] =        {NULL,          parse_binary, Prec_Factor},
  [Tk_Bang] =         {parse_unary,   NULL,         Prec_Unary},
  [Tk_Bang_Equal] =   {NULL,          parse_binary, Prec_Equality},
  [Tk_Equal] =        {NULL,          NULL,         Prec_None},
  [Tk_Plus_Equal] =   {NULL,          NULL,         Prec_None},
  [Tk_Minus_Equal] =  {NULL,          NULL,         Prec_None},
  [Tk_Star_Equal] =   {NULL,          NULL,         Prec_None},
  [Tk_Slash_Equal] =  {NULL,          NULL,         Prec_None},
  [Tk_Equal_Equal] =  {NULL,          parse_binary, Prec_Equality},
  [Tk_Less] =         {NULL,          parse_binary, Prec_Comparison},
  [Tk_Less_Equal] =   {NULL,          parse_binary, Prec_Comparison},
  [Tk_Greater] =      {NULL,          parse_binary, Prec_Comparison},
  [Tk_Greater_Equal] ={NULL,          parse_binary, Prec_Comparison},
  [Tk_And] =          {NULL,          NULL,         Prec_None},
  [Tk_Or] =           {NULL,          NULL,         Prec_None},

  [Tk_Identifier] =   {parse_ident,   NULL,         Prec_None},
  [Tk_String] =       {parse_string,  NULL,         Prec_None},
  [Tk_Number] =       {parse_number,  NULL,         Prec_None},
  [Tk_True] =         {parse_literal, NULL,         Prec_None},
  [Tk_False] =        {parse_literal, NULL,         Prec_None},
  [Tk_Null] =         {parse_literal, NULL,         Prec_None},

  [Tk_For] =          {NULL,          NULL,         Prec_None},
  [Tk_While] =        {NULL,          NULL,         Prec_None},
  [Tk_If] =           {NULL,          NULL,         Prec_None},
  [Tk_Else] =         {NULL,          NULL,         Prec_None},
  [Tk_Print] =        {NULL,          NULL,         Prec_None},
  [Tk_Def] =          {NULL,          NULL,         Prec_None},
  [Tk_Return] =       {NULL,          NULL,         Prec_None},
  [Tk_Let] =          {NULL,          NULL,         Prec_None},
};

static void parse_binary(Env* env, bool assignable) {
  (void)assignable;
  i32 op_kind = parser.previous.kind;
  parse_expr(env, rules[op_kind].rbp);

  switch(op_kind) {
    case Tk_Plus:         emit_byte(env, Op_Add); break;
    case Tk_Minus:        emit_byte(env, Op_Sub); break;
    case Tk_Star:         emit_byte(env, Op_Mul); break;
    case Tk_Slash:        emit_byte(env, Op_Div); break;
    case Tk_Less:         emit_byte(env, Op_Less); break;
    case Tk_Greater:      emit_byte(env, Op_Greater); break;
    case Tk_Equal_Equal:  emit_byte(env, Op_Equal); break;
    case Tk_Less_Equal:
      emit_byte(env, Op_Greater);
      emit_byte(env, Op_Not);
      break;
    case Tk_Greater_Equal:
      emit_byte(env, Op_Less);
      emit_byte(env, Op_Not);
      break;
    case Tk_Bang_Equal:
      emit_byte(env, Op_Equal);
      emit_byte(env, Op_Not);
      break;
    default: return;
  }
}

static void parse_expr(Env* env, i32 lbp) {
  parser.previous = parser.current;
  parser.current = next_token();
  Parse_Fn prefix = rules[parser.previous.kind].prefix;
  if(prefix == NULL) {
    error("Expected an expression");
    return;
  }
  bool assignable = lbp <= Prec_Assign;
  prefix(env, assignable);
  while(lbp < rules[parser.current.kind].rbp) {
    parser.previous = parser.current;
    parser.current = next_token();
    Parse_Fn mixfix = rules[parser.previous.kind].mixfix;
    mixfix(env, assignable);
  }
}

static void parse_print_stmt(Env* env) {
  parse_expr(env, Prec_Assign);
  consume_token(Tk_Semicolon, "Expect ';' after expression");
  emit_byte(env, Op_Print);
}
static void parse_expr_stmt(Env* env) {
  parse_expr(env, Prec_Assign);
  consume_token(Tk_Semicolon, "Expect ';' after expression");
  emit_byte(env, Op_Pop);
}

static void parse_stmt(Env* env) {
  if(match_token(Tk_Print)) 
    parse_print_stmt(env);
  else
    parse_expr_stmt(env);
}

static void add_local(Token name) {
  if(locals_info.count == 16) {
    error("Too many local variables in a function");
    return;
  }
  Local* local = &locals_info.locals[locals_info.count];
  locals_info.count += 1;
  local->name = name;

  // uninitalized state
  local->active_on = -1;
}

static void declare_variable(void) {
  Token* name = &parser.previous;
  if(locals_info.scope_depth == 0) return;

  for(int x = locals_info.count -1; x >= 0; x -= 1) {
    Local* local = &locals_info.locals[x];
    if(local->active_on != -1 && local->active_on < locals_info.scope_depth)
      break;

    if(identifiers_equal(name, &local->name)) {
      error("Multiple definitions of the same variable exists");
    }
  }

  add_local(*name);
}

static uint8_t parse_variable(Env* env, char* error_descr) {
  consume_token(Tk_Identifier, error_descr);

  declare_variable();
  if(locals_info.scope_depth > 0) return 0;

  return identifier_constant(env, &parser.previous);
}

static void mark_var_initialized(uint8_t idx) {
  locals_info.locals[idx].active_on = locals_info.scope_depth;
}

static void define_variable(Env* env, uint8_t idx, uint32_t local_idx) {
  if(locals_info.scope_depth > 0) {
    mark_var_initialized(local_idx);
    return;
  }
  emit_opcode_with_operand(env, Op_Define_Global, idx);
}

static void parse_var_decl(Env* env) {
  uint8_t count = 0, ids[4];
  ids[count] = parse_variable(env, "Expect variable name");
  count += 1;
  while(match_token(Tk_Comma)) {
    ids[count] = parse_variable(env, "Expect variable name");
    count += 1;
  }

  uint8_t x = 0;
  if(match_token(Tk_Equal)) {
    parse_expr(env, Prec_Assign);
    define_variable(env, ids[x], x);
    x += 1;

    while(match_token(Tk_Comma)) {
      parse_expr(env, Prec_Assign);
      define_variable(env, ids[x], x);
      x += 1;
    }
  }
  else
    emit_byte(env, Op_Null);
  consume_token(Tk_Semicolon, "Expect ';' after expression");
}

static void parse_decl(Env* env);
static void parse_block(Env* env) {
  while(!check_token(Tk_Right_Brace) && !check_token(Tk_Eof)) {
    parse_decl(env);
  }
  consume_token(Tk_Right_Brace, "Expect '}' after block");
}

static void end_scope(Env* env) { 
  locals_info.scope_depth -= 1;

  while(locals_info.count > 0 &&
    locals_info.locals[locals_info.count -1].active_on > locals_info.scope_depth) {
    emit_byte(env, Op_Pop);
    locals_info.count -= 1;
  }
}
static void parse_decl(Env* env) {
  if(match_token(Tk_Let)) {
    parse_var_decl(env);
  }
  else if(match_token(Tk_Left_Brace)) {
    locals_info.scope_depth += 1;
    parse_block(env);
    end_scope(env);
  }
  else {
    parse_stmt(env);
  }

  if(parser.panic_mode)
    parser_sync();
}

bool parse_and_gen_bytecode(Env* env, char* src) {
  (void)env;
  set_lexer_state(src);
  advance_token(); // just to load first token in parser.current
  parser.had_error = false;
  parser.panic_mode = false;
  locals_info.count = 0;
  locals_info.scope_depth = 0;

  while(!match_token(Tk_Eof))
    parse_decl(env);

  consume_token(Tk_Eof, "Expected end of expression");
  emit_byte(env, Op_Return);
  return !parser.had_error;
}