#import "./chaos_parser.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

typedef enum {
  PRIM_I8,
  PRIM_I16,
  PRIM_I32,
  PRIM_I64,
  PRIM_U8,
  PRIM_U16,
  PRIM_U32,
  PRIM_U64,
  PRIM_F32,
  PRIM_F64,
  PRIM_BOOL,
  PRIM_VOID,
  PRIM_UNKNOWN
} Chaos_Primitive_Type;

struct Chaos_Type;

struct Array_Data {
  std::shared_ptr<Chaos_Type> element;
  size_t size;
};

struct Function_Data {
  std::vector<std::shared_ptr<Chaos_Type>> params;
  std::shared_ptr<Chaos_Type> ret;
};

struct Struct_Field {
  std::string name;
  std::shared_ptr<Chaos_Type> type;
};

struct Struct_Data {
  std::string name;
  std::vector<Struct_Field> fields;
};

struct Enum_Data {
  std::string name;
  std::vector<std::string> values;
};

struct Void_Data {};

using Type_Data =
    std::variant<Void_Data, Chaos_Primitive_Type, Array_Data, Function_Data,
                 std::shared_ptr<Chaos_Type>, Struct_Data, Enum_Data>;

typedef struct Chaos_Type {
  enum {
    TYPE_PRIMITIVE,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_POINTER,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_STRING,
    TYPE_VOID
  } kind;

  Type_Data data;

  static std::shared_ptr<Chaos_Type> make_void() {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_VOID;
    t->data = Void_Data{};
    return t;
  }

  static std::shared_ptr<Chaos_Type> make_string() {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_STRING;
    return t;
  }

  static std::shared_ptr<Chaos_Type> make_primitive(Chaos_Primitive_Type p) {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_PRIMITIVE;
    t->data = p;
    return t;
  }

  static std::shared_ptr<Chaos_Type>
  make_array(std::shared_ptr<Chaos_Type> elem, size_t sz) {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_ARRAY;
    t->data = Array_Data{elem, sz};
    return t;
  }

  static std::shared_ptr<Chaos_Type>
  make_pointer(std::shared_ptr<Chaos_Type> pointee) {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_POINTER;
    t->data = pointee;
    return t;
  }

  static std::shared_ptr<Chaos_Type>
  make_function(std::vector<std::shared_ptr<Chaos_Type>> params,
                std::shared_ptr<Chaos_Type> ret) {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_FUNCTION;
    t->data = Function_Data{std::move(params), ret};
    return t;
  }

  static std::shared_ptr<Chaos_Type>
  make_struct(std::string name, std::vector<Struct_Field> fields) {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_STRUCT;
    t->data = Struct_Data{std::move(name), std::move(fields)};
    return t;
  }

  static std::shared_ptr<Chaos_Type>
  make_enum(std::string name, std::vector<std::string> values) {
    auto t = std::make_shared<Chaos_Type>();
    t->kind = TYPE_ENUM;
    t->data = Enum_Data{std::move(name), std::move(values)};
    return t;
  }

  Chaos_Primitive_Type &primitive() {
    return std::get<Chaos_Primitive_Type>(data);
  }
  const Chaos_Primitive_Type &primitive() const {
    return std::get<Chaos_Primitive_Type>(data);
  }

  Array_Data &array() { return std::get<Array_Data>(data); }
  const Array_Data &array() const { return std::get<Array_Data>(data); }

  Function_Data &function() { return std::get<Function_Data>(data); }
  const Function_Data &function() const {
    return std::get<Function_Data>(data);
  }

  std::shared_ptr<Chaos_Type> &pointer() {
    return std::get<std::shared_ptr<Chaos_Type>>(data);
  }
  const std::shared_ptr<Chaos_Type> &pointer() const {
    return std::get<std::shared_ptr<Chaos_Type>>(data);
  }

  Struct_Data &structure() { return std::get<Struct_Data>(data); }
  const Struct_Data &structure() const { return std::get<Struct_Data>(data); }

  Enum_Data &enumeration() { return std::get<Enum_Data>(data); }
  const Enum_Data &enumeration() const { return std::get<Enum_Data>(data); }

  bool is_primitive() const { return kind == TYPE_PRIMITIVE; }
  bool is_array() const { return kind == TYPE_ARRAY; }

  bool is_numeric() const {
    if (!is_primitive())
      return false;
    switch (primitive()) {
    case PRIM_I8:
    case PRIM_I16:
    case PRIM_I32:
    case PRIM_I64:
    case PRIM_U8:
    case PRIM_U16:
    case PRIM_U32:
    case PRIM_U64:
    case PRIM_F32:
    case PRIM_F64:
      return true;
    default:
      return false;
    }
  }

  bool is_integer() const {
    if (!is_primitive())
      return false;
    switch (primitive()) {
    case PRIM_I8:
    case PRIM_I16:
    case PRIM_I32:
    case PRIM_I64:
    case PRIM_U8:
    case PRIM_U16:
    case PRIM_U32:
    case PRIM_U64:
      return true;
    default:
      return false;
    }
  }

  bool is_float() const {
    return is_primitive() &&
           (primitive() == PRIM_F32 || primitive() == PRIM_F64);
  }

  template <typename Visitor> auto visit(Visitor &&v) {
    return std::visit(std::forward<Visitor>(v), data);
  }

  template <typename Visitor> auto visit(Visitor &&v) const {
    return std::visit(std::forward<Visitor>(v), data);
  }

  size_t size_bytes() const {
    switch (kind) {
    case TYPE_PRIMITIVE:
      switch (primitive()) {
      case PRIM_I8:
        return 1;
      case PRIM_I16:
        return 2;
      case PRIM_I32:
        return 4;
      case PRIM_I64:
        return 8;
      case PRIM_F32:
        return 4;
      case PRIM_F64:
        return 8;
      case PRIM_BOOL:
        return 1;
      default:
        return 0;
      }

    case TYPE_POINTER:
      return 8;

    case TYPE_ARRAY:
      return array().size * array().element->size_bytes();

    case TYPE_STRING:
      return sizeof(size_t) + sizeof(const char *);

    case TYPE_STRUCT: {
      size_t size = 0;
      for (auto &f : structure().fields)
        size += f.type->size_bytes();
      return size;
    }

    default:
      return 0;
    }
  }
} Chaos_Type;

typedef enum {
  SYM_VARIABLE,
  SYM_CONSTANT,
  SYM_FUNCTION,
  SYM_PARAMETER,
  SYM_TYPE,
} Chaos_Symbol_Kind;

typedef struct Chaos_Symbol {
  Chaos_Symbol_Kind kind;
  std::string_view name;
  Chaos_Type type;

  // for Functions
  std::vector<Chaos_Type> param_types;
  Chaos_Type return_type;

  int scope_depth;
  Chaos_Symbol(Chaos_Symbol_Kind k, std::string_view n, Chaos_Type t)
      : kind(k), name(n), type(t), return_type(*Chaos_Type::make_void()),
        scope_depth(0) {}
} Chaos_Symbol;

enum IR_Type_Kind {
  IR_I8,
  IR_I16,
  IR_I32,
  IR_I64,
  IR_U8,
  IR_U16,
  IR_U32,
  IR_U64,
  IR_F32,
  IR_F64,
  IR_BOOL,
  IR_PTR,
  IR_STR,
  IR_VOID
};

struct IR_Type {
  IR_Type_Kind kind;
};

enum IR_Op {
  IR_NOP,

  IR_CONST_INT,
  IR_CONST_FLOAT,
  IR_CONST_STRING,

  IR_LOAD,
  IR_STORE,
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,

  IR_CMP_EQ,
  IR_CMP_LT,
  IR_CMP_GT,

  IR_NEG,

  IR_JMP,
  IR_JMP_IF_FALSE,
  IR_LABEL,

  IR_CALL,
  IR_INTRINSIC_PRINT,
  IR_RET
};

using IR_Value = int;

struct IR_Inst {
  IR_Op op;
  IR_Type type;

  IR_Value dst;
  IR_Value a;
  IR_Value b;

  std::string name;
  std::vector<IR_Value> args;
  std::vector<IR_Type> arg_types;

  int64_t int_value;
  double float_value;
  std::string string_value;
};

struct IR_Local {
  std::string name;
  IR_Type type;
  size_t stack_bytes = 0;
};

struct IR_Function {
  std::string name;

  std::vector<IR_Local> params;
  std::vector<IR_Local> locals;

  IR_Type return_type;

  std::vector<IR_Inst> code;
  std::vector<IR_Type> temp_types;

  int next_temp = 0;

  IR_Value new_temp(IR_Type type) {
    temp_types.push_back(type);
    return next_temp++;
  }
};

struct IR_Program {
  std::vector<IR_Function> functions;
};
