#import "./chaos_semantic.h"

struct Lowering_Context {
  IR_Function *fn;

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
      return {IR_F64};
    if (node->kind == AST_IDENT)
      return {IR_I32};
  }
  return {IR_I32};
}

IR_Type lower_type_name(std::string_view name) {
  if (name == "int" || name == "i32")
    return {IR_I32};
  if (name == "i64")
    return {IR_I64};
  if (name == "f32")
    return {IR_F32};
  if (name == "f64")
    return {IR_F64};
  if (name == "bool")
    return {IR_BOOL};
  if (name == "void")
    return {IR_VOID};

  assert(false && "Unknown type name");
  return {IR_I32};
}

IR_Value lower_expr(Chaos_AST *node, Lowering_Context &ctx) {
  if (node->kind == AST_INT) {
    IR_Value t = ctx.fn->new_temp();

    IR_Inst inst{};
    inst.op = IR_CONST_INT;
    inst.dst = t;
    inst.int_value = std::stoll(std::string(node->literal));
    inst.type = {IR_I32};
    ctx.fn->code.push_back(inst);

    return t;
  }
  if (node->kind == AST_CALL) {
    std::vector<IR_Value> arg_values;
    for (Chaos_AST *arg : node->call.args) {
      arg_values.push_back(lower_expr(arg, ctx));
    }

    std::string fn_name;
    if (node->call.caller->kind == AST_IDENT) {
      fn_name = std::string(node->call.caller->ident);
    } else {
      return ctx.fn->new_temp();
    }

    IR_Value t = ctx.fn->new_temp();
    IR_Inst inst{};
    inst.op = IR_CALL;
    inst.dst = t;
    inst.name = fn_name;
    inst.args = arg_values;
    inst.type = node->resolved_type ? lower_type(*node->resolved_type)
                                    : IR_Type{IR_I32};
    ctx.fn->code.push_back(inst);
    return t;
  }
  if (node->kind == AST_IDENT) {
    IR_Value t = ctx.fn->new_temp();

    IR_Inst inst{};
    inst.op = IR_LOAD;
    inst.dst = t;
    inst.name = node->ident;
    inst.type = get_expr_type(node);

    ctx.fn->code.push_back(inst);

    return t;
  }
  if (node->kind == AST_BINARY) {
    IR_Value left = lower_expr(node->binary.l, ctx);
    IR_Value right = lower_expr(node->binary.r, ctx);

    IR_Value t = ctx.fn->new_temp();

    IR_Inst inst{};
    inst.dst = t;
    inst.a = left;
    inst.b = right;
    inst.type = get_expr_type(node);

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
    ctx.fn->locals.push_back(local);

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
  fn.name = std::string(fn_node->function.name);
  fn.return_type = lower_type_name(fn_node->function.return_type);

  for (auto &p : fn_node->function.params) {
    IR_Local local;
    local.name = std::string(p.first);
    local.type = lower_type_name(p.second);
    fn.params.push_back(local);
  }

  Lowering_Context ctx;
  ctx.fn = &fn;

  lower_stmt(fn_node->function.body, ctx);

  return fn;
}

IR_Program lower_program(Chaos_AST *program) {
  IR_Program ir;

  for (auto *stmt : program->block.statements) {
    if (stmt->kind == AST_FUNCTION) {
      ir.functions.push_back(lower_function(stmt));
    }
  }

  return ir;
}
