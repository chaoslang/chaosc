#pragma once

#include "./chaos_backend.h"
#include <sstream>
#include <unordered_map>

class JavaScriptBackend {
private:
  std::stringstream output;
  std::unordered_map<int, std::string> temp_names;
  int temp_counter = 0;
  int indent_level = 0;

  std::string indent() { return std::string(indent_level * 2, ' '); }

  std::string get_temp_name(IR_Value v) {
    if (temp_names.find(v) == temp_names.end()) {
      temp_names[v] = "t" + std::to_string(temp_counter++);
    }
    return temp_names[v];
  }

  void emit_inst(const IR_Inst &inst) {
    switch (inst.op) {
    case IR_CONST_INT:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << inst.int_value << ";\n";
      break;

    case IR_CONST_FLOAT:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << inst.float_value << ";\n";
      break;

    case IR_ADD:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " + " << get_temp_name(inst.b)
             << ";\n";
      break;
    case IR_CONST_STRING:
      output << indent() << "let " << get_temp_name(inst.dst)
             << " = { len: " << inst.string_value.size() << ", data: " << '"'
             << inst.string_value << '"' << " };\n";
      break;
    case IR_SUB:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " - " << get_temp_name(inst.b)
             << ";\n";
      break;

    case IR_MUL:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " * " << get_temp_name(inst.b)
             << ";\n";
      break;

    case IR_DIV:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " / " << get_temp_name(inst.b)
             << ";\n";
      break;

    case IR_CMP_LT:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " < " << get_temp_name(inst.b)
             << ";\n";
      break;

    case IR_CMP_GT:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " > " << get_temp_name(inst.b)
             << ";\n";
      break;

    case IR_CMP_EQ:
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << get_temp_name(inst.a) << " === " << get_temp_name(inst.b)
             << ";\n";
      break;

    case IR_CALL: {
      output << indent() << "let " << get_temp_name(inst.dst) << " = "
             << inst.name << "(";
      for (size_t i = 0; i < inst.args.size(); i++) {
        output << get_temp_name(inst.args[i]);
        if (i + 1 < inst.args.size())
          output << ", ";
      }
      output << ");\n";
      break;
    }

    case IR_INTRINSIC_PRINT: {
      output << indent() << "console.log(";
      for (size_t i = 0; i < inst.args.size(); i++) {
        if (inst.arg_types[i].kind == IR_STR) {
          output << get_temp_name(inst.args[i]) << ".data";
        } else {
          output << get_temp_name(inst.args[i]);
        }
        if (i + 1 < inst.args.size())
          output << ", ";
      }
      output << ");\n";
      break;
    }
    case IR_RET:
      output << indent() << "return " << get_temp_name(inst.a) << ";\n";
      break;

    case IR_STORE:
      if (!inst.name.empty()) {
        output << indent() << inst.name << " = " << get_temp_name(inst.a)
               << ";\n";
      } else {
        output << indent() << "__mem[" << get_temp_name(inst.a)
               << "] = " << get_temp_name(inst.b) << ";\n";
      }
      break;

    case IR_LOAD:
      if (!inst.name.empty()) {
        output << indent() << "let " << get_temp_name(inst.dst) << " = "
               << inst.name << ";\n";
      } else {
        output << indent() << "let " << get_temp_name(inst.dst) << " = (__mem["
               << get_temp_name(inst.a) << "] ?? 0);\n";
      }
      break;

    default:
      break;
    }
  }

  std::string find_previous_label(const std::vector<IR_Inst> &code, int i) {
    for (int k = i; k >= 0; k--) {
      if (code[k].op == IR_LABEL)
        return code[k].name;
    }
    return "";
  }

  void emit_instructions_with_control_flow(const std::vector<IR_Inst> &code) {
    size_t first_label = code.size();

    for (size_t i = 0; i < code.size(); i++) {
      if (code[i].op == IR_LABEL) {
        first_label = i;
        break;
      }
    }

    for (size_t i = 0; i < first_label; i++)
      emit_inst(code[i]);

    if (first_label == code.size())
      return;

    std::string entry = code[first_label].name;

    output << indent() << "let __pc = \"" << entry << "\";\n";
    output << indent() << "while (true) {\n";
    indent_level++;

    output << indent() << "switch(__pc) {\n";
    indent_level++;

    bool case_open = false;

    for (size_t i = first_label; i < code.size(); i++) {
      const auto &inst = code[i];

      if (inst.op == IR_LABEL) {
        if (case_open) {
          indent_level--;
          output << indent() << "}\n";
        }

        output << indent() << "case \"" << inst.name << "\": {\n";
        indent_level++;
        case_open = true;
        continue;
      }

      switch (inst.op) {

      case IR_JMP:
        output << indent() << "__pc = \"" << inst.name << "\";\n";
        output << indent() << "break;\n";
        break;

      case IR_JMP_IF_FALSE:
        output << indent() << "if (!" << get_temp_name(inst.a) << ") {\n";
        indent_level++;
        output << indent() << "__pc = \"" << inst.name << "\";\n";
        output << indent() << "break;\n";
        indent_level--;
        output << indent() << "}\n";
        break;

      case IR_RET:
        output << indent() << "return " << get_temp_name(inst.a) << ";\n";
        break;

      default:
        emit_inst(inst);
        break;
      }
    }

    if (case_open) {
      indent_level--;
      output << indent() << "}\n";
    }

    indent_level--;
    output << indent() << "}\n";

    indent_level--;
    output << indent() << "}\n";
  }

public:
  void codegen(const IR_Program &ir) {
    output << "const __mem = Object.create(null);\n";
    output << "let __heap = 0;\n\n";
    for (const auto &fn : ir.functions) {
      output << "function " << fn.name << "(";

      for (size_t i = 0; i < fn.params.size(); i++) {
        output << fn.params[i].name;
        if (i + 1 < fn.params.size())
          output << ", ";
      }

      output << ") {\n";
      indent_level++;

      for (const auto &local : fn.locals)
        output << indent() << "let " << local.name << ";\n";

      for (const auto &local : fn.locals) {
        if (local.type.kind == IR_PTR && local.stack_bytes > 0) {
          output << indent() << local.name << " = __heap;\n";
          output << indent() << "__heap += " << local.stack_bytes << ";\n";
        }
      }

      emit_instructions_with_control_flow(fn.code);

      indent_level--;
      output << "}\n\n";
    }

    output << "main();\n";
  }

  std::string get_output() const { return output.str(); }
};
