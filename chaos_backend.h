#import "./chaos_semantic.h"
#include "chaos_parser.h"
#include <cstddef>
#include <iostream>
#include <unordered_map>

struct Lowering_Context {
  IR_Function *fn;
  std::unordered_map<std::string, IR_Type> value_types;
  static std::unordered_map<std::string, Struct_Data> named_structs;
  std::unordered_map<std::string, std::string> value_struct_types;
  static std::unordered_map<std::string, IR_Type> named_types;
  static std::unordered_map<std::string, IR_Type> named_function_returns;

  int label_counter = 0;

  std::string new_label() { return "L" + std::to_string(label_counter++); }
};

IR_Type lower_type(const Chaos_Type &t) {
  if (t.kind == Chaos_Type::TYPE_PRIMITIVE) {
    switch (t.primitive()) {
    case PRIM_I32:
      return {IR_I32};
    case PRIM_I64:
      return {IR_I64};
    case PRIM_F32:
      return {IR_F32};
    case PRIM_F64:
      return {IR_F64};
    case PRIM_BOOL:
      return {IR_BOOL};
    default:
      break;
    }
  }

  if (t.kind == Chaos_Type::TYPE_POINTER)
    return {IR_PTR};

  if (t.kind == Chaos_Type::TYPE_VOID)
    return {IR_VOID};

  if (t.kind == Chaos_Type::TYPE_ENUM)
    return {IR_I32};

  if (t.kind == Chaos_Type::TYPE_STRUCT)
    return {IR_PTR};

  assert(false && "Unsupported type?");
  return {IR_VOID};
}

IR_Type get_expr_type(Chaos_AST *node) {
  if (node && node->resolved_type) {
    return lower_type(*node->resolved_type);
  }
  if (node) {
    if (node->kind == AST_INT)
      return {IR_I32};
    if (node->kind == AST_FLOAT)
      return {IR_F32};
    if (node->kind == AST_IDENT)
      return {IR_I32};
    if (node->kind == AST_STRING)
      return {IR_STR};
  }
  return {IR_I32};
}

size_t struct_field_offset(const Struct_Data &s, const std::string &name) {
  size_t offset = 0;

  for (auto &f : s.fields) {
    if (f.name == name)
      return offset;

    offset += f.type->size_bytes();
  }

  assert(false && "Unknown struct field");
  return 0;
}

size_t struct_type_size_bytes(const Struct_Data &s) {
  size_t n = 0;
  for (const auto &f : s.fields)
    n += f.type->size_bytes();
  return n;
}

const Struct_Data &member_object_struct_data(Chaos_AST *object,
                                             Lowering_Context &ctx) {
  if (object->kind == AST_IDENT) {
    auto it = ctx.value_struct_types.find(std::string(object->ident));
    assert(it != ctx.value_struct_types.end() &&
           "Unknown struct-typed variable in member access");

    auto sit = Lowering_Context::named_structs.find(it->second);
    assert(sit != Lowering_Context::named_structs.end() &&
           "Unknown struct type in member access");

    return sit->second;
  }

  assert(false && "Unsupported member base expression");
  return Lowering_Context::named_structs.begin()->second;
}

std::shared_ptr<Chaos_Type> struct_field_type(const Struct_Data &s,
                                              std::string_view field) {
  for (auto &f : s.fields) {
    if (f.name == field)
      return f.type;
  }

  assert(false && "Unknown struct field");
  return nullptr;
}

IR_Type promote_numeric_type(IR_Type a, IR_Type b) {
  if (a.kind == IR_F64 || b.kind == IR_F64)
    return {IR_F64};
  if (a.kind == IR_F32 || b.kind == IR_F32)
    return {IR_F32};
  return {IR_I32};
}

IR_Type lower_type_name(std::string_view name) {
  if (name == "int" || name == "i32")
    return {IR_I32};
  if (name == "i64")
    return {IR_I64};
  if (name == "f32" || name == "float")
    return {IR_F32};
  if (name == "f64")
    return {IR_F64};
  if (name == "bool")
    return {IR_BOOL};
  if (name == "void")
    return {IR_VOID};
  if (name == "string")
    return {IR_STR};

  auto it = Lowering_Context::named_types.find(std::string(name));
  if (it != Lowering_Context::named_types.end())
    return it->second;

  std::cout << name << std::endl;
  assert(false && "Unknown type name");
  return {IR_I32};
}

std::string struct_type_name_for_expr(Chaos_AST *expr, Lowering_Context &ctx) {
  if (expr->kind == AST_IDENT) {
    auto it = ctx.value_struct_types.find(std::string(expr->ident));
    if (it != ctx.value_struct_types.end())
      return it->second;
  }
  return "";
}

IR_Value lower_expr(Chaos_AST *node, Lowering_Context &ctx) {
  if (node->kind == AST_INT) {
    IR_Value t = ctx.fn->new_temp({IR_I32});

    IR_Inst inst{};
    inst.op = IR_CONST_INT;
    inst.dst = t;
    inst.int_value = std::stoll(std::string(node->literal));
    inst.type = {IR_I32};
    ctx.fn->code.push_back(inst);

    return t;
  }
  if (node->kind == AST_MEMBER) {
    IR_Value base = lower_expr(node->member.object, ctx);

    const Struct_Data &struct_data =
        member_object_struct_data(node->member.object, ctx);

    size_t offset =
        struct_field_offset(struct_data, std::string(node->member.field));

    IR_Value off = ctx.fn->new_temp({IR_I32});

    IR_Inst c{};
    c.op = IR_CONST_INT;
    c.dst = off;
    c.int_value = offset;
    c.type = {IR_I32};
    ctx.fn->code.push_back(c);

    IR_Value addr = ctx.fn->new_temp({IR_PTR});

    IR_Inst add{};
    add.op = IR_ADD;
    add.dst = addr;
    add.a = base;
    add.b = off;
    add.type = {IR_PTR};
    ctx.fn->code.push_back(add);

    auto member_type = struct_field_type(struct_data, node->member.field);
    IR_Type field_type = lower_type(*member_type);

    IR_Value result = ctx.fn->new_temp(field_type);

    IR_Inst load{};
    load.op = IR_LOAD;
    load.dst = result;
    load.a = addr;
    load.type = field_type;

    ctx.fn->code.push_back(load);

    return result;
  }
  if (node->kind == AST_STRING) {
    IR_Value t = ctx.fn->new_temp({IR_STR});

    IR_Inst inst{};
    inst.op = IR_CONST_STRING;
    inst.dst = t;
    inst.string_value = std::string(node->literal);
    inst.type = {IR_STR};
    ctx.fn->code.push_back(inst);

    return t;
  }
  if (node->kind == AST_FLOAT) {
    IR_Value t = ctx.fn->new_temp({IR_F32});

    IR_Inst inst{};
    inst.op = IR_CONST_FLOAT;
    inst.dst = t;
    inst.float_value = std::stof(std::string(node->literal));
    inst.type = {IR_F32};
    ctx.fn->code.push_back(inst);

    return t;
  }
  if (node->kind == AST_CALL) {
    std::vector<IR_Value> arg_values;
    std::string fn_name;

    if (node->call.caller->kind == AST_IDENT) {
      fn_name = std::string(node->call.caller->ident);
      for (Chaos_AST *arg : node->call.args) {
        arg_values.push_back(lower_expr(arg, ctx));
      }
    } else if (node->call.caller->kind == AST_MEMBER) {
      Chaos_AST *member = node->call.caller;
      std::string owner = struct_type_name_for_expr(member->member.object, ctx);
      assert(!owner.empty() && "method receiver must be a struct");
      fn_name = owner + "." + std::string(member->member.field);

      arg_values.push_back(lower_expr(member->member.object, ctx));
      for (Chaos_AST *arg : node->call.args) {
        arg_values.push_back(lower_expr(arg, ctx));
      }
    } else {
      return ctx.fn->new_temp({IR_I32});
    }
    IR_Inst inst{};
    inst.args = arg_values;

    if (fn_name == "print") {
      std::vector<IR_Type> arg_types;

      for (IR_Value val : arg_values) {
        arg_types.push_back(ctx.fn->temp_types[val]);
      }

      IR_Value t = ctx.fn->new_temp({IR_I32});
      IR_Inst inst{};
      inst.op = IR_INTRINSIC_PRINT;
      inst.args = arg_values;
      inst.arg_types = arg_types;
      inst.dst = t;
      inst.type = {IR_I32};
      ctx.fn->code.push_back(inst);

      return t;
    }

    IR_Type call_type = {IR_I32};
    auto fit = Lowering_Context::named_function_returns.find(fn_name);
    if (fit != Lowering_Context::named_function_returns.end()) {
      call_type = fit->second;
    } else if (node->resolved_type) {
      call_type = lower_type(*node->resolved_type);
    }

    IR_Value t = ctx.fn->new_temp(call_type);

    inst.op = IR_CALL;
    inst.dst = t;
    inst.name = fn_name;
    inst.type = call_type;

    ctx.fn->code.push_back(inst);
    return t;
  }
  if (node->kind == AST_IDENT) {
    IR_Type value_type = {IR_I32};
    auto it = ctx.value_types.find(std::string(node->ident));
    if (it != ctx.value_types.end()) {
      value_type = it->second;
    }

    IR_Value t = ctx.fn->new_temp(value_type);

    IR_Inst inst{};
    inst.op = IR_LOAD;
    inst.dst = t;
    inst.name = node->ident;
    inst.type = value_type;

    ctx.fn->code.push_back(inst);

    return t;
  }
  if (node->kind == AST_BINARY) {
    IR_Value left = lower_expr(node->binary.l, ctx);
    IR_Value right = lower_expr(node->binary.r, ctx);

    IR_Type left_type = ctx.fn->temp_types[left];
    IR_Type right_type = ctx.fn->temp_types[right];
    IR_Type result_type = promote_numeric_type(left_type, right_type);
    if (node->binary.op == TOK_LT || node->binary.op == TOK_GT ||
        node->binary.op == TOK_EQEQ) {
      result_type = {IR_BOOL};
    }
    IR_Value t = ctx.fn->new_temp(result_type);

    IR_Inst inst{};
    inst.dst = t;
    inst.a = left;
    inst.b = right;
    inst.type = result_type;

    switch (node->binary.op) {
    case TOK_PLUS:
      inst.op = IR_ADD;
      break;
    case TOK_MINUS:
      inst.op = IR_SUB;
      break;
    case TOK_STAR:
      inst.op = IR_MUL;
      break;
    case TOK_SLASH:
      inst.op = IR_DIV;
      break;
    case TOK_LT:
      inst.op = IR_CMP_LT;
      break;
    case TOK_GT:
      inst.op = IR_CMP_GT;
      break;
    case TOK_EQEQ:
      inst.op = IR_CMP_EQ;
      break;
    default:
      assert(false);
    }

    ctx.fn->code.push_back(inst);
    return t;
  }
  assert(false && "lower_expr: unhandled AST node kind");
  return 0;
}

void lower_stmt(Chaos_AST *node, Lowering_Context &ctx) {
  if (node->kind == AST_RETURN) {
    IR_Value val = lower_expr(node->unary.expr, ctx);

    IR_Inst inst{};
    inst.op = IR_RET;
    inst.a = val;

    ctx.fn->code.push_back(inst);
  }
  if (node->kind == AST_ASIGN) {

    if (node->assign.target->kind == AST_MEMBER) {
      Chaos_AST *mem = node->assign.target;
      IR_Value base = lower_expr(mem->member.object, ctx);

      const Struct_Data &s = member_object_struct_data(mem->member.object, ctx);
      size_t offset = struct_field_offset(s, std::string(mem->member.field));

      IR_Value off = ctx.fn->new_temp({IR_I32});
      IR_Inst c{};
      c.op = IR_CONST_INT;
      c.dst = off;
      c.int_value = (int64_t)offset;
      c.type = {IR_I32};
      ctx.fn->code.push_back(c);

      IR_Value addr = ctx.fn->new_temp({IR_PTR});
      IR_Inst add{};
      add.op = IR_ADD;
      add.dst = addr;
      add.a = base;
      add.b = off;
      add.type = {IR_PTR};
      ctx.fn->code.push_back(add);

      IR_Value value = lower_expr(node->assign.value, ctx);

      IR_Inst store{};
      store.op = IR_STORE;
      store.a = addr;
      store.b = value;
      store.type = ctx.fn->temp_types[value];
      ctx.fn->code.push_back(store);

      return;
    }

    IR_Value value = lower_expr(node->assign.value, ctx);

    IR_Inst inst{};
    inst.op = IR_STORE;
    inst.name = node->assign.target->ident;
    inst.a = value;

    ctx.fn->code.push_back(inst);
  }
  if (node->kind == AST_IF) {
    std::string else_label = ctx.new_label();
    std::string end_label = ctx.new_label();

    IR_Value cond = lower_expr(node->if_stmt.cond, ctx);

    IR_Inst jmp{};
    jmp.op = IR_JMP_IF_FALSE;
    jmp.a = cond;
    jmp.name = else_label;
    ctx.fn->code.push_back(jmp);

    lower_stmt(node->if_stmt.then_br, ctx);

    IR_Inst jmp_end{};
    jmp_end.op = IR_JMP;
    jmp_end.name = end_label;
    ctx.fn->code.push_back(jmp_end);

    ctx.fn->code.push_back({IR_LABEL, {}, 0, 0, 0, else_label});

    if (node->if_stmt.else_br)
      lower_stmt(node->if_stmt.else_br, ctx);

    ctx.fn->code.push_back({IR_LABEL, {}, 0, 0, 0, end_label});
  }
  if (node->kind == AST_WHILE) {
    std::string loop_start = ctx.new_label();
    std::string loop_end = ctx.new_label();

    ctx.fn->code.push_back({IR_LABEL, {}, 0, 0, 0, loop_start});

    IR_Value cond = lower_expr(node->while_stmt.cond, ctx);

    IR_Inst jmp{};
    jmp.op = IR_JMP_IF_FALSE;
    jmp.a = cond;
    jmp.name = loop_end;
    ctx.fn->code.push_back(jmp);

    lower_stmt(node->while_stmt.body, ctx);

    IR_Inst jmp_back{};
    jmp_back.op = IR_JMP;
    jmp_back.name = loop_start;
    ctx.fn->code.push_back(jmp_back);

    ctx.fn->code.push_back({IR_LABEL, {}, 0, 0, 0, loop_end});
    return;
  }

  if (node->kind == AST_VAR_DECL) {
    IR_Local local;
    local.name = std::string(node->var_decl.name);
    local.type = lower_type_name(node->var_decl.type);

    auto struct_it =
        Lowering_Context::named_structs.find(std::string(node->var_decl.type));
    if (struct_it != Lowering_Context::named_structs.end()) {
      ctx.value_struct_types[local.name] = std::string(node->var_decl.type);
      local.stack_bytes = struct_type_size_bytes(struct_it->second);
    }

    ctx.fn->locals.push_back(local);
    ctx.value_types[local.name] = local.type;

    if (node->var_decl.init) {
      IR_Value val = lower_expr(node->var_decl.init, ctx);
      IR_Inst store{};
      store.op = IR_STORE;
      store.name = node->var_decl.name;
      store.a = val;
      ctx.fn->code.push_back(store);
    }
    return;
  }
  if (node->kind == AST_BLOCK) {
    for (Chaos_AST *stmt : node->block.statements) {
      lower_stmt(stmt, ctx);
    }
    return;
  }
  if (node->kind == AST_CALL || node->kind == AST_BINARY ||
      node->kind == AST_UNARY || node->kind == AST_IDENT) {

    lower_expr(node, ctx);
    return;
  }
}

IR_Function lower_function(Chaos_AST *fn_node) {
  IR_Function fn;

  fn.name = fn_node->function.owner.empty()
                ? std::string(fn_node->function.name)
                : std::string(fn_node->function.owner) + "." +
                      std::string(fn_node->function.name);
  fn.return_type = lower_type_name(fn_node->function.return_type);

  for (auto &p : fn_node->function.params) {
    IR_Local local;
    local.name = std::string(p.first);
    local.type = lower_type_name(p.second);
    fn.params.push_back(local);
  }

  Lowering_Context ctx;
  ctx.fn = &fn;

  for (const auto &param : fn.params) {
    ctx.value_types[param.name] = param.type;
  }

  for (const auto &param : fn_node->function.params) {
    if (Lowering_Context::named_structs.find(std::string(param.second)) !=
        Lowering_Context::named_structs.end()) {
      ctx.value_struct_types[std::string(param.first)] =
          std::string(param.second);
    }
  }

  lower_stmt(fn_node->function.body, ctx);

  return fn;
}
std::unordered_map<std::string, IR_Type> Lowering_Context::named_types;
std::unordered_map<std::string, Struct_Data> Lowering_Context::named_structs;
std::unordered_map<std::string, IR_Type>
    Lowering_Context::named_function_returns;

IR_Program lower_program(Chaos_AST *program) {
  IR_Program ir;

  for (auto *stmt : program->block.statements) {
    if (stmt->kind == AST_STRUCT) {
      Struct_Data s;
      s.name = std::string(stmt->struct_decl.name);
      for (const auto &field : stmt->struct_decl.fields) {
        Struct_Field f;
        f.name = std::string(field.first);

        if (field.second == "int" || field.second == "i32") {
          f.type = Chaos_Type::make_primitive(PRIM_I32);
        } else if (field.second == "i64") {
          f.type = Chaos_Type::make_primitive(PRIM_I64);
        } else if (field.second == "float" || field.second == "f32") {
          f.type = Chaos_Type::make_primitive(PRIM_F32);
        } else if (field.second == "f64") {
          f.type = Chaos_Type::make_primitive(PRIM_F64);
        } else if (field.second == "bool") {
          f.type = Chaos_Type::make_primitive(PRIM_BOOL);
        } else {
          assert(false && "Unsupported struct field type");
        }

        s.fields.push_back(std::move(f));
      }
      Lowering_Context::named_structs[s.name] = std::move(s);
      Lowering_Context::named_types[std::string(stmt->struct_decl.name)] = {
          IR_PTR};
      continue;
    }

    if (stmt->kind == AST_ENUM) {
      Lowering_Context::named_types[std::string(stmt->enum_decl.name)] = {
          IR_I32};
      continue;
    }

    if (stmt->kind == AST_FUNCTION) {
      std::string fn_name = stmt->function.owner.empty()
                                ? std::string(stmt->function.name)
                                : std::string(stmt->function.owner) + "." +
                                      std::string(stmt->function.name);
      Lowering_Context::named_function_returns[fn_name] =
          lower_type_name(stmt->function.return_type);
    }
  }

  for (auto *stmt : program->block.statements) {
    if (stmt->kind == AST_FUNCTION) {
      ir.functions.push_back(lower_function(stmt));
    }
  }

  return ir;
}
