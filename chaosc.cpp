#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#define CHAOS_LEXER_IMPLEMENTATION
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

static bool write_file(const std::string &path, const std::string &contents) {
  std::ofstream out(path, std::ios::binary);
  if (!out)
    return false;
  out << contents;
  return out.good();
}

static bool c_to_exe(const std::string &c_source,
                                    const std::string &exe_path) {
  char temp_path[] = "/tmp/chaoscXXXXXX";
  int fd = mkstemp(temp_path);
  if (fd == -1) {
    std::perror("mkstemp");
    return false;
  }
  close(fd);

  std::string base_path = temp_path;
  std::string c_path = base_path + ".c";

  try {
    std::filesystem::rename(base_path, c_path);
  } catch (const std::exception &e) {
    std::cerr << "rename temp file failed: " << e.what() << "\n";
    std::filesystem::remove(base_path);
    return false;
  }

  if (!write_file(c_path, c_source)) {
    std::cerr << "failed to write temp C file\n";
    std::filesystem::remove(c_path);
    return false;
  }

  std::string cmd = "gcc \"" + c_path + "\" -o \"" + exe_path + "\"";
  int rc = std::system(cmd.c_str());

  std::filesystem::remove(c_path);
  return rc == 0;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::fprintf(stderr,
                 "Usage: %s <source.ch> <output> <backend>\n"
                 "Backends: c, js, exe\n",
                 argv[0]);
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

  IR_Program ir = lower_program(ast);
  std::string backend = argv[3];
  std::string out;

  if (backend == "js") {
    JavaScriptBackend js_backend;
    js_backend.codegen(ir);
    out = js_backend.get_output() + "\n";

    if (!write_file(argv[2], out)) {
      std::cerr << "failed to write output file\n";
      return 1;
    }
    return 0;
  }

  if (backend == "c") {
    CBackend c_backend;
    c_backend.codegen(ir);
    out = c_backend.get_output() + "\n";

    if (!write_file(argv[2], out)) {
      std::cerr << "failed to write output file\n";
      return 1;
    }
    return 0;
  }

  if (backend == "exe") {
    CBackend c_backend;
    c_backend.codegen(ir);
    out = c_backend.get_output() + "\n";

    if (!c_to_exe(out, argv[2])) {
      std::cerr << "compilation failed\n";
      return 1;
    }
    return 0;
  }

  std::cerr << "unknown backend: " << backend << "\n";
  return 1;
}
