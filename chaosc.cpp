#include <cstdio>
#include <fstream>
#define CHAOS_LEXER_IMPLEMENTATION
#include <iostream>
#include <string>

#include "./chaos_backend_c.h"
#include "./chaos_backend_js.h"

std::string read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("open failed");

  file.seekg(0, std::ios::end);
  std::size_t size = file.tellg();
  file.seekg(0);

  std::string buffer(size, '\0');
  file.read(buffer.data(), size);

  return buffer;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::fprintf(stderr, "Usage: %s <source.ch> <output> <backend>\n", argv[0]);
    return 1;
  }

  std::string source;
  try {
    source = read_file(argv[1]);
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }

  Chaos_Lexer lexer = {0};
  chaos_lexer_run(&lexer, source);

  Chaos_Parser parser(&lexer.tokens);
  Chaos_AST *ast = parse_program(&parser);
  if (!ast) {
    std::fprintf(stderr, "Failed to parse\n");
    return 1;
  }

  std::string out = "";
  IR_Program ir = lower_program(ast);

  if (std::string(argv[3]) == "js") {
    JavaScriptBackend js_backend;
    js_backend.codegen(ir);
    out = js_backend.get_output() + "\n";
  } else if (std::string(argv[3]) == "c") {
    CBackend c_backend;
    c_backend.codegen(ir);
    out = c_backend.get_output() + "\n";
  }
  std::ofstream output(argv[2]);
  output << out;
  return 0;
}
