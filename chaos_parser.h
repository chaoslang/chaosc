#import "./chaos_lexer.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

typedef enum {
  AST_INT,
  AST_FLOAT,
  AST_STRING,

  AST_BINARY,
  AST_UNARY,
  AST_CALL,
  AST_INDEX,

  AST_VAR_DECL,
  AST_BLOCK,
  AST_IF,
  AST_WHILE,
  AST_RETURN,
  AST_FUNCTION,
  AST_STRUCT,
  AST_ENUM,
  AST_IDENT,
  AST_ASIGN,
  AST_MEMBER,

  AST_PROGRAM
} Chaos_AST_Kind;

struct Chaos_Type;

typedef struct Chaos_AST {
  Chaos_AST_Kind kind;

  std::shared_ptr<Chaos_Type> resolved_type;

  std::string_view literal;

  std::string_view ident;

  struct {
    Chaos_Token_Kind op;
    Chaos_AST *l;
    Chaos_AST *r;
  } binary;

  struct {
    Chaos_Token_Kind op;
    Chaos_AST *expr;
  } unary;

  struct {
    std::vector<Chaos_AST *> statements;
  } block;

  struct {
    Chaos_AST *object;
    std::string_view field;
  } member;

  struct {
    Chaos_AST *cond;
    Chaos_AST *then_br;
    Chaos_AST *else_br;
  } if_stmt;

  struct {
    Chaos_AST *cond;
    Chaos_AST *body;
  } while_stmt;

  struct {
    std::string_view name;
    std::vector<std::pair<std::string_view, std::string_view>> params;
    std::string_view owner; // owner struct for methods
    std::string_view return_type;
    Chaos_AST *body;
  } function;

  struct {
    Chaos_AST *caller;
    std::vector<Chaos_AST *> args;
  } call;

  struct {
    std::string_view name;
    std::string_view type;
    Chaos_AST *init;
  } var_decl;

  struct {
    Chaos_AST *target;
    Chaos_AST *value;
  } assign;

  struct {
    std::string_view name;
    std::vector<std::pair<std::string_view, std::string_view>> fields;
  } struct_decl;

  struct {
    std::string_view name;
    std::vector<std::string_view> items;
  } enum_decl;

  Chaos_AST()
      : kind(AST_PROGRAM), literal(), ident(),
        binary{TOK_INT, nullptr, nullptr}, unary{TOK_INT, nullptr}, block(),
        if_stmt{nullptr, nullptr, nullptr}, while_stmt{nullptr, nullptr},
        function{std::string_view{},
                 {},
                 std::string_view{},
                 std::string_view{},
                 nullptr},
        call{nullptr, {}},
        var_decl{std::string_view{}, std::string_view{}, nullptr},
        assign{nullptr, nullptr}, enum_decl{std::string_view{}, {}},
        struct_decl{std::string_view{}, {}}, member{nullptr, std::string_view{}}

  {}
} Chaos_AST;

typedef struct Chaos_Parser {
  Chaos_Tokens *tokens;
  size_t pos;

  Chaos_Parser(Chaos_Tokens *tokens = NULL, size_t pos = 0)
      : tokens(tokens), pos(pos) {}

  Chaos_Token *peek() { return &tokens->items[pos]; }

  Chaos_Token *advance() { return &tokens->items[pos++]; }

  bool match(Chaos_Token_Kind kind) {
    if (peek()->kind == kind) {
      advance();
      return true;
    }
    return false;
  }
} Chaos_Parser;

Chaos_AST *parse_expression(Chaos_Parser *p);
Chaos_AST *parse_term(Chaos_Parser *p);
Chaos_AST *parse_factor(Chaos_Parser *p);
Chaos_AST *parse_statement(Chaos_Parser *p);

Chaos_AST *parse_primary(Chaos_Parser *p) {
  Chaos_Token *tok = p->peek();

  if (p->match(TOK_INT)) {
    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_INT;
    node->literal = tok->text;
    return node;
  }
  if (p->match(TOK_FLOAT)) {
    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_FLOAT;
    node->literal = tok->text;
    return node;
  }
  if (p->match(TOK_STRING)) {
    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_STRING;
    node->literal = tok->text;
    return node;
  }

  if (p->match(TOK_IDENT)) {
    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_IDENT;
    node->ident = tok->text;
    return node;
  }

  if (p->match(TOK_LPAREN)) {
    Chaos_AST *expr = parse_expression(p);
    if (!p->match(TOK_RPAREN))
      std::fprintf(stderr, "Expected closing ')'\n");
    return expr;
  }
  return nullptr;
}

Chaos_AST *parse_var_decl(Chaos_Parser *p) {
  if (!p->match(TOK_VAR))
    return nullptr;

  if (p->peek()->kind != TOK_IDENT) {
    std::fprintf(stderr, "Expected variable name\n");
    return nullptr;
  }

  std::string_view name = p->advance()->text;

  if (!p->match(TOK_COLON)) {
    std::fprintf(stderr, "Expected ':' after variable name\n");
    return nullptr;
  }

  if (p->peek()->kind != TOK_IDENT) {
    std::fprintf(stderr, "Expected type name\n");
    return nullptr;
  }

  std::string_view type = p->advance()->text;

  Chaos_AST *init = nullptr;

  if (p->match(TOK_EQUAL)) {
    init = parse_expression(p);
  }
  if (!p->match(TOK_SEMI)) {
    std::fprintf(stderr, "Expected ';' after variable declaration\n");
  }

  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_VAR_DECL;
  node->var_decl.name = name;
  node->var_decl.type = type;
  node->var_decl.init = init;

  return node;
}

Chaos_AST *parse_postfix(Chaos_Parser *p, Chaos_AST *left) {
  if (!left)
    return nullptr;

  while (true) {

    if (p->peek()->kind == TOK_LPAREN) {
      p->advance();

      Chaos_AST *call_node = new Chaos_AST();
      call_node->kind = AST_CALL;
      call_node->call.caller = left;

      if (p->peek()->kind != TOK_RPAREN) {
        call_node->call.args.push_back(parse_expression(p));
        while (p->match(TOK_COMMA)) {
          call_node->call.args.push_back(parse_expression(p));
        }
      }

      if (!p->match(TOK_RPAREN)) {
        std::fprintf(stderr, "Expected ')' after function arguments\n");
      }

      left = call_node;
      continue;
    }

    if (p->match(TOK_DOT)) {
      if (p->peek()->kind != TOK_IDENT) {
        std::fprintf(stderr, "Expected field name after '.'\n");
        return left;
      }

      Chaos_AST *node = new Chaos_AST();
      node->kind = AST_MEMBER;
      node->member.object = left;
      node->member.field = p->advance()->text;

      left = node;
      continue;
    }

    break;
  }

  return left;
}

Chaos_AST *parse_factor(Chaos_Parser *p) {
  if (p->peek()->kind == TOK_MINUS) {
    Chaos_Token *tok = p->advance();
    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_UNARY;
    node->unary.op = tok->kind;
    node->unary.expr = parse_factor(p);
    return node;
  }
  Chaos_AST *primary = parse_primary(p);
  return parse_postfix(p, primary);
}

Chaos_AST *parse_term(Chaos_Parser *p) {
  Chaos_AST *left = parse_factor(p);

  while (p->peek()->kind == TOK_PLUS || p->peek()->kind == TOK_MINUS) {
    Chaos_Token_Kind op = p->advance()->kind;
    Chaos_AST *right = parse_factor(p);

    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_BINARY;
    node->binary.l = left;
    node->binary.op = op;
    node->binary.r = right;

    left = node;
  }

  return left;
}

Chaos_AST *parse_multiplicative(Chaos_Parser *p) {
  Chaos_AST *left = parse_factor(p);

  while (p->peek()->kind == TOK_STAR || p->peek()->kind == TOK_SLASH) {
    Chaos_Token_Kind op = p->advance()->kind;
    Chaos_AST *right = parse_factor(p);

    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_BINARY;
    node->binary.l = left;
    node->binary.op = op;
    node->binary.r = right;

    left = node;
  }

  return left;
}

Chaos_AST *parse_additive(Chaos_Parser *p) {
  Chaos_AST *left = parse_multiplicative(p);

  while (p->peek()->kind == TOK_PLUS || p->peek()->kind == TOK_MINUS) {
    Chaos_Token_Kind op = p->advance()->kind;
    Chaos_AST *right = parse_multiplicative(p);

    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_BINARY;
    node->binary.l = left;
    node->binary.op = op;
    node->binary.r = right;

    left = node;
  }

  return left;
}

Chaos_AST *parse_comparison(Chaos_Parser *p) {
  Chaos_AST *left = parse_additive(p);

  while (p->peek()->kind == TOK_GT || p->peek()->kind == TOK_LT ||
         p->peek()->kind == TOK_EQEQ || p->peek()->kind == TOK_NOT) {
    Chaos_Token_Kind op = p->advance()->kind;
    Chaos_AST *right = parse_additive(p);

    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_BINARY;
    node->binary.l = left;
    node->binary.op = op;
    node->binary.r = right;

    left = node;
  }

  return left;
}

Chaos_AST *parse_assignment(Chaos_Parser *p) {
  Chaos_AST *left = parse_comparison(p);

  if (p->match(TOK_EQUAL)) {
    Chaos_AST *value = parse_assignment(p);
    Chaos_AST *node = new Chaos_AST();
    node->kind = AST_ASIGN;
    node->assign.target = left;
    node->assign.value = value;

    return node;
  }

  return left;
}

Chaos_AST *parse_expression(Chaos_Parser *p) { return parse_assignment(p); }

Chaos_AST *parse_if(Chaos_Parser *p) {
  if (!p->match(TOK_IF)) {
    std::fprintf(stderr, "Expected 'fn'\n");
    return nullptr;
  }
  if (!p->match(TOK_LPAREN)) {
    std::fprintf(stderr, "Expected: '('\n");
    return nullptr;
  }
  Chaos_AST *cond = parse_expression(p);

  if (!p->match(TOK_RPAREN)) {
    std::fprintf(stderr, "Expected ')'\n");
    return nullptr;
  }
  Chaos_AST *then_branch = nullptr;
  if (p->match(TOK_LCURLY)) {
    then_branch = new Chaos_AST();
    then_branch->kind = AST_BLOCK;
    while (!p->match(TOK_RCURLY)) {
      then_branch->block.statements.push_back(parse_statement(p));
    }
  } else {
    then_branch = parse_statement(p);
  }

  Chaos_AST *else_branch = nullptr;

  if (p->match(TOK_ELSE)) {
    if (p->match(TOK_LCURLY)) {
      else_branch = new Chaos_AST();
      else_branch->kind = AST_BLOCK;

      while (p->peek()->kind != TOK_RCURLY && p->peek()->kind != TOK_EOF) {
        else_branch->block.statements.push_back(parse_statement(p));
      }

      if (!p->match(TOK_RCURLY)) {
        std::fprintf(stderr, "Expected '}' after else block\n");
      }
    } else {
      else_branch = parse_statement(p);
    }
  }
  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_IF;
  node->if_stmt.cond = cond;
  node->if_stmt.then_br = then_branch;
  node->if_stmt.else_br = else_branch;
  return node;
}

Chaos_AST *parse_return(Chaos_Parser *p) {
  if (!p->match(TOK_RETURN)) {
    return nullptr;
  }

  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_RETURN;

  node->unary.expr = parse_expression(p);

  if (!p->match(TOK_SEMI)) {
    std::fprintf(stderr, "Expected ';' after return\n");
  }

  return node;
}

Chaos_AST *parse_while(Chaos_Parser *p) {
  if (!p->match(TOK_WHILE)) {
    std::fprintf(stderr, "Expected 'while'\n");
    return nullptr;
  }

  if (!p->match(TOK_LPAREN)) {
    std::fprintf(stderr, "Expected: '('\n");
    return nullptr;
  }
  Chaos_AST *cond = parse_expression(p);

  if (!p->match(TOK_RPAREN)) {
    std::fprintf(stderr, "Expected: ')'\n");
    return nullptr;
  }

  Chaos_AST *body = nullptr;
  if (p->match(TOK_LCURLY)) {
    body = new Chaos_AST();
    body->kind = AST_BLOCK;
    while (!p->match(TOK_RCURLY)) {
      body->block.statements.push_back(parse_statement(p));
    }
  } else {
    body = parse_statement(p);
  }
  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_WHILE;
  node->while_stmt.cond = cond;
  node->while_stmt.body = body;
  return node;
}

Chaos_AST *parse_struct(Chaos_Parser *p) {
  if (!p->match(TOK_STRUCT)) {
    std::fprintf(stderr, "Expected: 'fn'\n");
    return nullptr;
  }
  if (p->peek()->kind != TOK_IDENT) {
    std::fprintf(stderr, "Expected struct name\n");
    return nullptr;
  }
  std::string_view name = p->advance()->text;

  if (!p->match(TOK_EQUAL)) {
    std::fprintf(stderr, "Expected: '=' after struct name\n");
    return nullptr;
  }

  if (!p->match(TOK_LCURLY)) {
    std::fprintf(stderr, "Expected: '{' after struct name\n");
    return nullptr;
  }

  std::vector<std::pair<std::string_view, std::string_view>> fields;

  while (p->peek()->kind != TOK_RCURLY && p->peek()->kind != TOK_EOF) {
    if (p->peek()->kind != TOK_IDENT) {
      std::fprintf(stderr, "Expected field name\n");
      return nullptr;
    }
    std::string_view field_name = p->advance()->text;

    if (!p->match(TOK_COLON)) {
      std::fprintf(stderr, "Expected ':' after field name\n");
      return nullptr;
    }

    if (p->peek()->kind != TOK_IDENT) {
      std::fprintf(stderr, "Expected field type\n");
      return nullptr;
    }

    std::string_view field_type = p->advance()->text;

    fields.push_back({field_name, field_type});

    if (!p->match(TOK_COMMA))
      break;
  }

  if (!p->match(TOK_RCURLY)) {
    std::fprintf(stderr, "Expected '}' after struct\n");
  }

  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_STRUCT;
  node->struct_decl.name = name;
  node->struct_decl.fields = std::move(fields);

  return node;
}

Chaos_AST *parse_function(Chaos_Parser *p) {
  if (!p->match(TOK_FN)) {
    std::fprintf(stderr, "Expected: 'fn'\n");
    return nullptr;
  }
  if (p->peek()->kind != TOK_IDENT) {
    std::fprintf(stderr, "Expected function name\n");
    return nullptr;
  }
  Chaos_Token *name_tok = p->advance();

  std::string_view owner_name;
  std::string_view fn_name = name_tok->text;

  if (p->match(TOK_DOT)) {
    owner_name = name_tok->text;
    if (p->peek()->kind != TOK_IDENT) {
      std::fprintf(stderr, "Expected method name after '.'\n");
      return nullptr;
    }
    fn_name = p->advance()->text;
  }

  if (!p->match(TOK_LPAREN)) {
    std::fprintf(stderr, "Expected: '('\n");
    return nullptr;
  }

  std::vector<std::pair<std::string_view, std::string_view>> params;
  while (p->peek()->kind != TOK_RPAREN) {
    if (p->peek()->kind != TOK_IDENT) {
      std::fprintf(stderr, "Expected: parameter name");
      return nullptr;
    }
    std::string_view param_name = p->advance()->text;
    if (!p->match(TOK_COLON)) {
      std::fprintf(stderr, "Expected `:` after argument name\n");
      return nullptr;
    }
    if (p->peek()->kind != TOK_IDENT) {
      std::fprintf(stderr, "Expected type name after ':'\n");
      return nullptr;
    }
    std::string_view param_type = p->advance()->text;

    params.push_back({param_name, param_type});

    if (!p->match(TOK_COMMA))
      break;
  }
  if (!p->match(TOK_RPAREN)) {
    std::fprintf(stderr, "Expected to close function\n");
    return nullptr;
  }

  if (!p->match(TOK_COLON)) {
    std::fprintf(stderr, "Expected `:` after function to indicate return type");
    return nullptr;
  }

  if (p->peek()->kind != TOK_IDENT) {
    std::fprintf(stderr, "Expected return type from function\n");
    return nullptr;
  }
  std::string_view return_type = p->advance()->text;

  if (!p->match(TOK_LCURLY)) {
    std::fprintf(stderr, "Expected '{' to begin function body\n");
    return nullptr;
  }

  Chaos_AST *body = new Chaos_AST();
  body->kind = AST_BLOCK;
  while (!p->match(TOK_RCURLY)) {
    body->block.statements.push_back(parse_statement(p));
  }

  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_FUNCTION;
  node->function.owner = owner_name;
  node->function.name = fn_name;
  node->function.params = std::move(params);
  node->function.return_type = return_type;
  node->function.body = body;

  return node;
}

Chaos_AST *parse_enum(Chaos_Parser *p) {
  if (!p->match(TOK_ENUM))
    return nullptr;

  if (p->peek()->kind != TOK_IDENT) {
    std::fprintf(stderr, "Expected enum name\n");
    return nullptr;
  }

  std::string_view name = p->advance()->text;

  if (!p->match(TOK_EQUAL)) {
    std::fprintf(stderr, "Expected '=' after enum name\n");
    return nullptr;
  }

  if (!p->match(TOK_LCURLY)) {
    std::fprintf(stderr, "Expected '{' after enum name\n");
    return nullptr;
  }

  std::vector<std::string_view> values;

  while (p->peek()->kind != TOK_RCURLY && p->peek()->kind != TOK_EOF) {
    if (p->peek()->kind != TOK_IDENT) {
      std::fprintf(stderr, "Expected enum value\n");
      return nullptr;
    }

    values.push_back(p->advance()->text);

    if (!p->match(TOK_COMMA))
      break;
  }

  if (!p->match(TOK_RCURLY)) {
    std::fprintf(stderr, "Expected '}' after enum\n");
  }

  Chaos_AST *node = new Chaos_AST();
  node->kind = AST_ENUM;
  node->enum_decl.name = name;
  node->enum_decl.items = std::move(values);

  return node;
}

Chaos_AST *parse_statement(Chaos_Parser *p) {
  if (p->peek()->kind == TOK_IF)
    return parse_if(p);
  if (p->peek()->kind == TOK_WHILE)
    return parse_while(p);
  if (p->peek()->kind == TOK_FN)
    return parse_function(p);
  if (p->peek()->kind == TOK_RETURN)
    return parse_return(p);
  if (p->peek()->kind == TOK_VAR)
    return parse_var_decl(p);
  if (p->peek()->kind == TOK_STRUCT)
    return parse_struct(p);
  if (p->peek()->kind == TOK_ENUM)
    return parse_enum(p);

  Chaos_AST *expr = parse_expression(p);

  if (!p->match(TOK_SEMI)) {
    std::fprintf(stderr, "Expected ';' after expression\n");
  }

  return expr;
}

Chaos_AST *parse_program(Chaos_Parser *p) {
  Chaos_AST *program = new Chaos_AST();
  program->kind = AST_PROGRAM;

  while (p->peek()->kind != TOK_EOF) {
    Chaos_AST *stmt = parse_statement(p);
    program->block.statements.push_back(stmt);
  }
  return program;
}
