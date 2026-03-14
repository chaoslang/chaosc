#include <string_view>
#include <vector>

#ifndef CHAOSDEF
#define CHAOSDEF
#endif

typedef enum {
  TOK_EOF = 0,

  TOK_INT,
  TOK_FLOAT,
  TOK_IDENT,
  TOK_STRING,

  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LCURLY,
  TOK_RCURLY,

  TOK_COMMA,
  TOK_DOT,
  TOK_COLON,
  TOK_SEMI,
  TOK_LSQUARE,
  TOK_RSQUARE,

  TOK_PLUS,
  TOK_MINUS,
  TOK_STAR,
  TOK_SLASH,
  TOK_EQUAL,
  TOK_FN,
  TOK_CALL,
  TOK_INDEX,
  TOK_RETURN,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_FOREACH,
  TOK_IN,

  TOK_GT,
  TOK_LT,
  TOK_EQEQ,
  TOK_JMP,
  TOK_JMP_FALSE,
  TOK_POP,
  TOK_IMPORT,
  TOK_EXTERN,
  TOK_FROM,
  TOK_BANG,
  TOK_NOT,
  TOK_DECLARE,
  TOK_ARRAY,
  TOK_VAR,
  TOK_STRUCT,
  TOK_ENUM,
  TOK_TYPE,
  TOK_CONST,

  TOK_UNKNOWN

} Chaos_Token_Kind;

typedef struct {
  Chaos_Token_Kind kind;
  std::string_view text;
  size_t line;
  size_t column;
  size_t jump;
} Chaos_Token;

typedef struct {
  std::vector<Chaos_Token> items;
} Chaos_Tokens;

typedef struct {
  size_t pos;
  size_t line;
  size_t column;
  Chaos_Tokens tokens;
} Chaos_Lexer;

CHAOSDEF Chaos_Token chaos_get_token(Chaos_Token_Kind kind,
                                     std::string_view text, size_t line,
                                     size_t column);
CHAOSDEF void chaos_lexer_init(Chaos_Lexer *lx);
CHAOSDEF void chaos_lexer_run(Chaos_Lexer *lx, std::string_view src);

CHAOSDEF void chaos_lexer_emit(Chaos_Lexer *lx, std::string_view src,
                               Chaos_Token_Kind kind, size_t start, size_t end,
                               size_t line, size_t column);

#ifdef CHAOS_LEXER_IMPLEMENTATION

CHAOSDEF Chaos_Token chaos_get_token(Chaos_Token_Kind kind,
                                     std::string_view text, size_t line,
                                     size_t column) {
  return (Chaos_Token){
      .kind = kind, .text = text, .line = line, .column = column};
}

CHAOSDEF void chaos_lexer_emit(Chaos_Lexer *lx, std::string_view src,
                               Chaos_Token_Kind kind, size_t start, size_t end,
                               size_t line, size_t column) {
  std::string_view text = src.substr(start, end - start);
  lx->tokens.items.push_back(chaos_get_token(kind, text, line, column));
}

CHAOSDEF void chaos_lexer_init(Chaos_Lexer *lx) {
  lx->pos = 0;
  lx->line = 1;
  lx->column = 1;
}

CHAOSDEF void chaos_lexer_run(Chaos_Lexer *lx, std::string_view src) {
  while (lx->pos < src.length()) {
    char c = src[lx->pos];

    if (c == ' ' || c == '\t' || c == '\r') {
      lx->pos++;
      lx->column++;
      continue;
    }

    if (c == '\n') {
      lx->pos++;
      lx->line++;
      lx->column = 1;
      continue;
    }

    size_t start = lx->pos;
    size_t line = lx->line;
    size_t column = lx->column;

    if ((c >= 'a' && c <= 'z') || c >= 'A' && c <= 'Z' || c == '_') {
      while (lx->pos < src.length()) {
        char ch = src[lx->pos];
        if (!(ch == '_' || (ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) {
          break;
        }
        lx->pos++;
        lx->column++;
      }
      std::string_view text = src.substr(start, lx->pos - start);

      if (text == "fn") {
        chaos_lexer_emit(lx, src, TOK_FN, start, lx->pos, line, column);
      } else if (text == "return") {
        chaos_lexer_emit(lx, src, TOK_RETURN, start, lx->pos, line, column);
      } else if (text == "if") {
        chaos_lexer_emit(lx, src, TOK_IF, start, lx->pos, line, column);
      } else if (text == "else") {
        chaos_lexer_emit(lx, src, TOK_ELSE, start, lx->pos, line, column);
      } else if (text == "while") {
        chaos_lexer_emit(lx, src, TOK_WHILE, start, lx->pos, line, column);
      } else if (text == "foreach") {
        chaos_lexer_emit(lx, src, TOK_FOREACH, start, lx->pos, line, column);
      } else if (text == "in") {
        chaos_lexer_emit(lx, src, TOK_IN, start, lx->pos, line, column);
      } else if (text == "import") {
        chaos_lexer_emit(lx, src, TOK_IMPORT, start, lx->pos, line, column);
      } else if (text == "extern") {
        chaos_lexer_emit(lx, src, TOK_EXTERN, start, lx->pos, line, column);
      } else if (text == "from") {
        chaos_lexer_emit(lx, src, TOK_FROM, start, lx->pos, line, column);
      } else if (text == "type") {
        chaos_lexer_emit(lx, src, TOK_DECLARE, start, lx->pos, line, column);
      } else if (text == "var") {
        chaos_lexer_emit(lx, src, TOK_VAR, start, lx->pos, line, column);
      } else if (text == "struct") {
        chaos_lexer_emit(lx, src, TOK_STRUCT, start, lx->pos, line, column);
      } else if (text == "const") {
        chaos_lexer_emit(lx, src, TOK_CONST, start, lx->pos, line, column);
      } else if (text == "enum") {
        chaos_lexer_emit(lx, src, TOK_ENUM, start, lx->pos, line, column);
      } else if (text == "type") {
        chaos_lexer_emit(lx, src, TOK_TYPE, start, lx->pos, line, column);
      } else {
        chaos_lexer_emit(lx, src, TOK_IDENT, start, lx->pos, line, column);
      }
      continue;
    }

    if (c >= '0' && c <= '9') {
      bool dot = false;
      while (lx->pos < src.length()) {
        char ch = src[lx->pos];

        if (ch == '.') {
          if (dot)
            break;
          dot = true;
        } else if (ch < '0' || ch > '9') {
          break;
        }
        lx->pos++;
        lx->column++;
      }
      chaos_lexer_emit(lx, src, dot ? TOK_FLOAT : TOK_INT, start, lx->pos, line,
                       column);
      continue;
    }

    if (c == '"') {
      lx->pos++;
      lx->column++;
      start = lx->pos;

      while (lx->pos < src.length() && src[lx->pos] != '"') {
        if (src[lx->pos] == '\n') {
          lx->line++;
          lx->column = 1;
        } else {
          lx->column++;
        }
        lx->pos++;
      }
      chaos_lexer_emit(lx, src, TOK_STRING, start, lx->pos, line, column);

      if (lx->pos < src.length()) {
        lx->pos++;
        lx->column++;
      }
      continue;
    }

    lx->pos++;
    lx->column++;
    Chaos_Token_Kind kind = TOK_UNKNOWN;

    switch (c) {
    case '(':
      kind = TOK_LPAREN;
      break;
    case ')':
      kind = TOK_RPAREN;
      break;
    case '{':
      kind = TOK_LCURLY;
      break;
    case '}':
      kind = TOK_RCURLY;
      break;
    case '.':
      kind = TOK_DOT;
      break;
    case ',':
      kind = TOK_COMMA;
      break;
    case ':':
      kind = TOK_COLON;
      break;
    case ';':
      kind = TOK_SEMI;
      break;
    case '[':
      kind = TOK_LSQUARE;
      break;
    case ']':
      kind = TOK_RSQUARE;
      break;
    case '+':
      kind = TOK_PLUS;
      break;
    case '-':
      kind = TOK_MINUS;
      break;
    case '*':
      kind = TOK_STAR;
      break;
    case '/':
      kind = TOK_SLASH;
      break;
    case '>':
      kind = TOK_GT;
      break;
    case '<':
      kind = TOK_LT;
      break;
    case '=':
      if (lx->pos < src.length() && src[lx->pos] == '=') {
        lx->pos++;
        lx->column++;
        kind = TOK_EQEQ;
      } else {
        kind = TOK_EQUAL;
      }
      break;
    case '!':
      if (lx->pos < src.length() && src[lx->pos] == '=') {
        lx->pos++;
        lx->column++;
        kind = TOK_NOT;
      } else {
        kind = TOK_BANG;
      }
      break;
    }

    chaos_lexer_emit(lx, src, kind, start, lx->pos, line, column);
  }
  chaos_lexer_emit(lx, src, TOK_EOF, lx->pos, lx->pos, lx->line, lx->column);
}
#endif // CHAOS_LEXER_IMPLEMENTATION
