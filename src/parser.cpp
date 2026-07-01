#include "parser.hpp"
#include "ast.hpp"
#include "containers.hpp"
#include "operator.hpp"
#include "print.hpp"
#include "symbol.hpp"
#include "token.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#define expect(ok, srcloc, msg)                                                \
  if (!(ok)) {                                                                 \
    std::ostringstream os;                                                     \
    os << msg;                                                                 \
    std::string cpp_str = os.str();                                            \
    String m = {.len = cpp_str.size(), .ptr = (uint8_t *)cpp_str.data()};      \
    parser->error_func(srcloc, m);                                             \
    parser->error_count += 1;                                                  \
    return nullptr;                                                            \
  }

#define expectEOF(ok) expect(ok, parser->cur_token.location, "Unexpected EOF");

#define expectToken(expected)                                                  \
  expect(parser->cur_token.kind == expected, parser->cur_token.location,       \
         "Expected: " << expected << ", Got: " << parser->cur_token.kind);

// Forward Declarations [
Node *parseExpr(ASTParser *parser, Precedence min_precedence, Symbol *scope,
                bool allow_init);
Node *parseField(ASTParser *parser, Node *name_prealloc, Symbol *scope);
Node *parseStmtCompound(ASTParser *parser, Symbol *scope);
// ] Forward Declarations

Node *parseInitializer(ASTParser *parser, Node *record, Symbol *scope) {
  Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->kind = NodeKind::Initializer;
  out->initializer.record = record;
  out->initializer.setters.init(parser->allocator, 8);

  expectToken(TokenKind::BlockBegin);
  expectEOF(parser->nextToken());

  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *setter = parseExpr(
        parser, (Precedence)((int32_t)Precedence::Assign + 1), scope, true);

    if (parser->cur_token.kind == TokenKind::Eq) {
      if (out->initializer.setters.length > 0) {
        if (out->initializer.is_list) {
          std::cerr << setter->location
                    << "Cannot mix designators with list initializers\n";
        }
      } else {
        out->initializer.is_list = false;
      }

      expect(setter->kind == NodeKind::Name, parser->cur_token.location,
             "Expected: Name, Got: " << setter->kind);
      setter->kind = NodeKind::Member;
      setter->member.name = setter->text;

      expectEOF(parser->nextToken());
      setter->member.value = parseExpr(parser, Precedence::Assign, scope, true);
    } else {
      if (out->initializer.setters.length > 0) {
        if (!out->initializer.is_list) {
          std::cerr << setter->location
                    << "Cannot mix list with designator initializers\n";
        }
      } else {
        out->initializer.is_list = true;
      }
    }

    setter->end_location = parser->cur_token.location;
    out->initializer.setters.push(setter);

    if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
      break;
    }
    expectEOF(parser->nextToken());
  }

  out->end_location = parser->cur_token.location;
  expectToken(TokenKind::BlockEnd);
  expectEOF(parser->nextToken());
  return out;
}

Node *parseBinaryExpr(ASTParser *parser, Precedence min_precedence, Node *atom,
                      Symbol *scope, bool allow_init) {
  Node *out = atom;

  while (true) {
    if (parser->cur_token.kind == TokenKind::ScopeBegin) {
      if (Precedence::Special < min_precedence) {
        break;
      }

      Node *_tmp = out;
      out = (Node *)parser->allocator->alloc(sizeof(Node));
      out->token = parser->cur_token;
      out->location = parser->cur_token.location;
      out->kind = NodeKind::Call;
      out->call.callee = _tmp;
      out->call.arguments.init(parser->allocator, 4);

      expectEOF(parser->nextToken());
      while (parser->cur_token.kind != TokenKind::ScopeEnd) {
        Node *arg = parseExpr(parser, Precedence::Assign, scope, true);
        expect(arg != nullptr, parser->cur_token.location,
               "Failed to parse expression");
        out->call.arguments.push(arg);

        if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
          break;
        }
        expectEOF(parser->nextToken());
      }

      expectToken(TokenKind::ScopeEnd);
      expectEOF(parser->nextToken());
      continue;
    } else if (parser->cur_token.kind == TokenKind::ArrayBegin) {
      if (Precedence::Special < min_precedence) {
        break;
      }

      Node *_tmp = out;
      out = (Node *)parser->allocator->alloc(sizeof(Node));
      out->token = parser->cur_token;
      out->location = parser->cur_token.location;
      out->kind = NodeKind::Index;
      out->index.slice = _tmp;

      expectEOF(parser->nextToken());
      out->index.index = parseExpr(parser, Precedence::Assign, scope, true);

      expectToken(TokenKind::ArrayEnd);
      expectEOF(parser->nextToken());
      continue;
    } else if (allow_init && parser->cur_token.kind == TokenKind::BlockBegin) {
      if (Precedence::Special < min_precedence) {
        break;
      }

      out = parseInitializer(parser, out, scope);
      expect(out != nullptr, parser->cur_token.location,
             "Failed to parse initializer");
      continue;
    } else if (parser->cur_token.kind == TokenKind::RangeLessThen ||
               parser->cur_token.kind == TokenKind::RangeEqualTo) {
      if (Precedence::Special < min_precedence) {
        break;
      }

      Node *_tmp = out;
      out = (Node *)parser->allocator->alloc(sizeof(Node));
      out->token = parser->cur_token;
      out->location = parser->cur_token.location;
      out->kind = NodeKind::Range;
      out->range.min = _tmp;

      out->range.mode = parser->cur_token.kind == TokenKind::RangeEqualTo
                            ? NodeRange::EqualTo
                            : NodeRange::LessThan;

      expectEOF(parser->nextToken());
      out->range.max = parseExpr(parser, Precedence::Assign, scope, false);

      expect(out->range.max != nullptr, parser->cur_token.location,
             "Failed to parse range max");
      continue;
    } else if (parser->cur_token.kind != TokenKind::Operator) {
      break;
    }

    Operator opcode = parser->cur_token._operator;
    Precedence precedence = operatorPrecedence(opcode);
    if (precedence < min_precedence) {
      break;
    }

    Associativity associativity = operatorAssociativity(opcode);
    precedence = (Precedence)((int32_t)precedence + (int32_t)associativity);

    Node *tmp_atom = out;
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->kind = NodeKind::Operator;
    out->token = parser->cur_token;
    out->_operator.opcode = opcode;
    out->_operator.lhs = tmp_atom;

    expectEOF(parser->nextToken());
    out->_operator.rhs = parseExpr(parser, precedence, scope, allow_init);
    expect(out->_operator.rhs != nullptr, out->location,
           "Failed to parse rhs expression");

    out->end_location = out->_operator.rhs->end_location;
  }

  return out;
}

struct FieldsAndBodyResult {
  bool ok;
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
};

FieldsAndBodyResult parseFieldsAndBody(ASTParser *parser, Symbol *scope) {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
  fields.init(parser->allocator, 8);
  body.init(parser->allocator, 8);

  bool allow_field = true;
  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *field = (Node *)parser->allocator->alloc(sizeof(Node));
    field->token = parser->cur_token;
    field->location = parser->cur_token.location;
    field->kind = NodeKind::Name;
    field->text = parser->cur_token.text;
    if (!parser->nextToken()) {
      return {false};
    }

    field = parseField(parser, field, scope);

    if (!field->field.definition) {
      if (!allow_field) {
        std::cerr << "Previous field didn't end with `,`\n";
        return {false};
      }

      field->end_location = parser->cur_token.location;
      fields.push(field);
      allow_field = parser->cur_token.kind == TokenKind::CommaDelimiter;
      if (allow_field && !parser->nextToken()) {
        return {false};
      }
      continue;
    }

    field->end_location = parser->cur_token.location;
    body.push(field);
    if (parser->cur_token.kind == TokenKind::LineDelimiter) {
      if (!parser->nextToken()) {
        return {false};
      }
    } else if (parser->prev_token.kind != TokenKind::BlockEnd) {
      std::cerr << "Statement must end with either `;` or `}`\n";
      return {false};
    }
  }

  return {true, fields, body};
}

FieldsAndBodyResult parseMembersAndBody(ASTParser *parser, Symbol *scope) {
  ArrayList<Node *> fields;
  ArrayList<Node *> body;
  fields.init(parser->allocator, 8);
  body.init(parser->allocator, 8);

  bool allow_member = true;
  while (parser->cur_token.kind != TokenKind::BlockEnd) {
    Node *field = (Node *)parser->allocator->alloc(sizeof(Node));
    field->token = parser->cur_token;
    field->location = parser->cur_token.location;
    field->kind = NodeKind::Name;
    field->text = parser->cur_token.text;
    if (!parser->nextToken()) {
      return {false};
    }

    if (parser->cur_token.kind != TokenKind::TypeSeperator) {
      if (!allow_member) {
        std::cerr << "Previous member didn't end with `,`\n";
        return {false};
      }

      field->kind = NodeKind::Member;
      field->member.name = field->text;
      field->member.value = nullptr;

      if (parser->cur_token.kind == TokenKind::Eq) {
        if (!parser->nextToken()) {
          return {false};
        }
        field->member.value =
            parseExpr(parser, Precedence::Assign, scope, true);
      }

      field->end_location = parser->cur_token.location;
      fields.push(field);

      Symbol *field_symbol = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
      field_symbol->init(parser->allocator, false, scope);
      field_symbol->node = field;

      allow_member = parser->cur_token.kind == TokenKind::CommaDelimiter;
      if (allow_member && !parser->nextToken()) {
        return {false};
      }

      continue;
    }

    field = parseField(parser, field, scope);
    body.push(field);

    field->end_location = parser->cur_token.location;
    if (parser->cur_token.kind == TokenKind::LineDelimiter) {
      if (!parser->nextToken()) {
        return {false};
      }
    } else if (parser->prev_token.kind != TokenKind::BlockEnd) {
      std::cerr << "Statement must end with either `;` or `}`\n";
      return {false};
    }
  }

  return {true, fields, body};
}

Node *parseExpr(ASTParser *parser, Precedence min_precedence, Symbol *scope,
                bool allow_init) {
  Node *out;

  if (parser->cur_token.kind != TokenKind::ScopeBegin) {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
  }

  switch (parser->cur_token.kind) {
  case TokenKind::Operator: {
    // Parse Unary
    out->kind = NodeKind::UnaryOperator;
    out->unary_operator.opcode = (UnaryOperator)parser->cur_token._operator;

    expectEOF(parser->nextToken());
    out->unary_operator.child =
        parseExpr(parser, Precedence::Unary, scope, allow_init);
    expect(out->unary_operator.child != nullptr, out->location,
           "Failed to parse child expression");
    break;
  }
  case TokenKind::Name: {
    out->kind = NodeKind::Name;
    out->text = parser->cur_token.text;
    parser->nextToken();
    break;
  }
  case TokenKind::Integer: {
    out->kind = NodeKind::Value;
    out->value.has_data = true;
    out->value.data.integer = parser->cur_token.integer;

    // Setup Type
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = true, .is_signed = out->value.data.integer < 0};
    out->value.type = parser->type_cache->get(t);

    parser->nextToken();
    break;
  }
  case TokenKind::Float: {
    out->kind = NodeKind::Value;
    out->value.has_data = true;
    out->value.data._float = parser->cur_token._float;

    // Setup Type
    Type t = {.kind = TypeKind::Float};
    t._float = {.is_untyped = true};
    out->value.type = parser->type_cache->get(t);

    parser->nextToken();
    break;
  }
  case TokenKind::Char: {
    out->kind = NodeKind::Value;
    out->value.has_data = true;
    out->value.data.integer = parser->cur_token.integer;

    // Setup Type
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = 8};
    out->value.type = parser->type_cache->get(t);

    parser->nextToken();
    break;
  }
  case TokenKind::String: {
    out->kind = NodeKind::Value;
    out->value.has_data = true;
    out->value.data.text = parser->cur_token.text;

    // Setup Type
    Type t = {.kind = TypeKind::Integer};
    t.integer = {.is_untyped = false, .is_signed = false, .bits = 8};
    out->value.type = parser->type_cache->get(t);

    t = {.kind = TypeKind::Slice};
    t.slice = {.length = (int64_t)out->value.data.text.len,
               .type = out->value.type};
    out->value.type = parser->type_cache->get(t);

    parser->nextToken();
    break;
  }
  case TokenKind::Function: {
    out->kind = NodeKind::Function;
    out->function.parameters.init(parser->allocator, 8);
    out->function.return_type = nullptr;
    out->function.body = nullptr;
    out->function.undefined = false;

    // Create Scope
    Symbol *fn_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    fn_scope->init(parser->allocator, true, scope);
    fn_scope->node = out;

    expectEOF(parser->nextToken());

    // Parse Parameters
    expectToken(TokenKind::ScopeBegin);
    expectEOF(parser->nextToken());
    while (parser->cur_token.kind != TokenKind::ScopeEnd) {
      Node *parameter = (Node *)parser->allocator->alloc(sizeof(Node));
      parameter->token = parser->cur_token;
      parameter->location = parser->cur_token.location;
      parameter->kind = NodeKind::Name;
      parameter->text = parser->cur_token.text;
      expectEOF(parser->nextToken());

      parameter = parseField(parser, parameter, fn_scope);
      out->function.parameters.push(parameter);

      if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
        break;
      }
      expectEOF(parser->nextToken());
    }
    expectToken(TokenKind::ScopeEnd);
    expectEOF(parser->nextToken());

    // Parse Return
    if (parser->cur_token.kind != TokenKind::BlockBegin &&
        parser->cur_token.kind != TokenKind::Undefined &&
        parser->cur_token.kind != TokenKind::LineDelimiter) {
      out->function.return_type =
          parseExpr(parser, Precedence::Assign, scope, false);
      expect(out->function.return_type != nullptr, out->location,
             "Failed to parse function return type");
    }

    // Parse Body
    if (parser->cur_token.kind == TokenKind::BlockBegin) {
      out->function.body = parseStmtCompound(parser, fn_scope);
      expect(out->function.body != nullptr, out->location,
             "Failed to parse function body");
    } else if (parser->cur_token.kind == TokenKind::Undefined) {
      expectEOF(parser->nextToken());
      out->function.undefined = true;
    }

    break;
  }
  case TokenKind::Struct: {
    out->kind = NodeKind::Struct;
    expectEOF(parser->nextToken());
    expectToken(TokenKind::BlockBegin);
    expectEOF(parser->nextToken());

    // Create Scope
    Symbol *record_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    record_scope->init(parser->allocator, false, scope);
    record_scope->node = out;

    // Parse Body
    FieldsAndBodyResult result = parseFieldsAndBody(parser, record_scope);
    expect(result.ok, out->location, "Failed to parse struct body");
    out->_struct.fields = result.fields;
    out->_struct.body = result.body;

    expectToken(TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  case TokenKind::Enum: {
    out->kind = NodeKind::Enum;
    out->_enum.repr_type = nullptr;

    expectEOF(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::BlockBegin) {
      out->_enum.repr_type =
          parseExpr(parser, Precedence::Assign, scope, false);
    }

    expectToken(TokenKind::BlockBegin);
    expectEOF(parser->nextToken());

    // Create Scope
    Symbol *record_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    record_scope->init(parser->allocator, false, scope);
    record_scope->node = out;

    // Parse Body
    FieldsAndBodyResult result = parseMembersAndBody(parser, record_scope);
    expect(result.ok, out->location, "Failed to parse enum body");
    out->_enum.members = result.fields;
    out->_enum.body = result.body;

    expectToken(TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  case TokenKind::Union: {
    out->kind = NodeKind::Union;
    out->_union.repr_type = nullptr;

    expectEOF(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::BlockBegin) {
      out->_union.repr_type =
          parseExpr(parser, Precedence::Assign, scope, false);
    }

    expectToken(TokenKind::BlockBegin);
    expectEOF(parser->nextToken());

    // Create Scope
    Symbol *record_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    record_scope->init(parser->allocator, false, scope);
    record_scope->node = out;

    // Parse Body
    FieldsAndBodyResult result = parseFieldsAndBody(parser, record_scope);
    expect(result.ok, out->location, "Failed to parse union body");
    out->_union.variants = result.fields;
    out->_union.body = result.body;

    expectToken(TokenKind::BlockEnd);
    parser->nextToken();
    break;
  }
  case TokenKind::Import: {
    out->kind = NodeKind::Import;
    expectEOF(parser->nextToken());
    expectToken(TokenKind::String);

    out->import.path = parser->cur_token.text;
    out->import.node = nullptr;
    out->import.scope = nullptr;
    expectEOF(parser->nextToken());
    parser->imports.push(out);
    break;
  }
  case TokenKind::Comptime: {
    out->kind = NodeKind::Comptime;
    expectEOF(parser->nextToken());
    out->child = parseExpr(parser, Precedence::Special, scope, allow_init);
    break;
  }
  case TokenKind::Const: {
    out->kind = NodeKind::Const;
    expectEOF(parser->nextToken());
    out->child = parseExpr(parser, Precedence::MemberAccess, scope, allow_init);
    break;
    break;
  }
  case TokenKind::ArrayBegin: {
    out->kind = NodeKind::Slice;
    out->slice.is_pointer = false;
    out->slice.length = nullptr;
    out->slice.type = nullptr;

    expectEOF(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::Operator &&
        parser->cur_token._operator == Operator::Mul) {
      out->slice.is_pointer = true;
      expectEOF(parser->nextToken());
    } else if (parser->cur_token.kind != TokenKind::ArrayEnd) {
      out->slice.length = parseExpr(parser, Precedence::Assign, scope, true);
    }

    expectToken(TokenKind::ArrayEnd);
    expectEOF(parser->nextToken());

    out->slice.type =
        parseExpr(parser, Precedence::MemberAccess, scope, allow_init);
    break;
  }
  case TokenKind::ScopeBegin: {
    expectEOF(parser->nextToken());
    out = parseExpr(parser, Precedence::Assign, scope, true);
    expectToken(TokenKind::ScopeEnd);
    expectEOF(parser->nextToken());
    break;
  }
  }

  out = parseBinaryExpr(parser, min_precedence, out, scope, allow_init);
  expect(out != nullptr, parser->cur_token.location,
         "Failed to parse expression");

  return out;
}

Node *parseAssignExpr(ASTParser *parser, Node *in, Symbol *scope) {
  Node *node = (Node *)parser->allocator->alloc(sizeof(Node));
  node->kind = NodeKind::Assignment;
  node->token = parser->cur_token;
  node->location = parser->cur_token.location;
  if (parser->cur_token.kind == TokenKind::Eq) {
    node->_operator.opcode = Operator::Assign;
  } else {
    node->_operator.opcode = parser->cur_token._operator;
  }
  node->_operator.lhs = in;

  expectEOF(parser->nextToken());
  node->_operator.rhs = parseExpr(parser, Precedence::Assign, scope, true);

  node->end_location = node->_operator.rhs->end_location;
  return node;
}

Node *parseNamespace(ASTParser *parser, Symbol *scope) {
  Symbol *namespace_symbol = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
  namespace_symbol->init(parser->allocator, false, scope);

  Node *out = parseStmtCompound(parser, scope);
  out->kind = NodeKind::Namespace;
  namespace_symbol->node = out;

  return out;
}

// The `name_prealloc` argument must contain the name of the variable, and is
// used as a preallocated node for the output
Node *parseField(ASTParser *parser, Node *name_prealloc, Symbol *scope) {
  Node *out = name_prealloc;
  expect(out->kind == NodeKind::Name, out->location,
         "Field expected Name, Got: " << out->kind);

  out->kind = NodeKind::Field;
  out->field.name = out->text;
  out->field.type = nullptr;
  out->field.initial = nullptr;
  out->field.attributes = nullptr;
  out->field.definition = false;

  // Check for duplicate field
  // Symbol *duplicate_symbol =
  //     scope->findSymbol(&out->field.name, &out->location);
  // if (duplicate_symbol != nullptr) {
  //   std::cerr << out->location << " Field with name `" << out->field.name
  //             << "` already exists within scope\n";
  //   return nullptr;
  // }

  // Create Symbol
  Symbol *field_symbol = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
  field_symbol->init(parser->allocator, false, scope);
  field_symbol->node = out;

  // Parse Type
  expectToken(TokenKind::TypeSeperator);
  expectEOF(parser->nextToken());
  if (parser->cur_token.kind != TokenKind::TypeSeperator &&
      parser->cur_token.kind != TokenKind::Eq) {
    out->field.type =
        parseExpr(parser, (Precedence)((int32_t)Precedence::Assign + 1),
                  field_symbol, false);
    expect(out->field.type != nullptr, out->location,
           "Failed to parse field type");
  }

  // Parse Initial
  out->field.definition = parser->cur_token.kind == TokenKind::TypeSeperator;
  if (out->field.definition || parser->cur_token.kind == TokenKind::Eq) {
    expectEOF(parser->nextToken());

    out->field.undefined = parser->cur_token.kind == TokenKind::Undefined;
    if (out->field.definition &&
        parser->cur_token.kind == TokenKind::BlockBegin) {
      out->field.initial = parseNamespace(parser, field_symbol);
      expect(out->field.initial != nullptr, out->location,
             "Failed to parse field initial");
    } else if (!out->field.undefined) {
      out->field.initial =
          parseExpr(parser, Precedence::Assign, field_symbol, true);
      expect(out->field.initial != nullptr, out->location,
             "Failed to parse field initial");
    } else {
      expectEOF(parser->nextToken());
    }
  }

  return out;
}

Node *parseConditional(ASTParser *parser, Symbol *scope) {
  if (parser->cur_token.kind == TokenKind::Name) {
    Node *node = (Node *)parser->allocator->alloc(sizeof(Node));
    node->kind = NodeKind::Name;
    node->token = parser->cur_token;
    node->location = parser->cur_token.location;
    node->text = parser->cur_token.text;

    expectEOF(parser->nextToken());

    if (parser->cur_token.kind == TokenKind::In) {
      Symbol *in_symbol = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
      in_symbol->init(parser->allocator, false, scope);
      in_symbol->node = node;

      expectEOF(parser->nextToken());

      node->kind = NodeKind::In;
      node->in.name = node->text;
      node->in.range = parseExpr(
          parser, (Precedence)((int32_t)Precedence::Assign + 1), scope, false);
    } else {
      node = parseBinaryExpr(parser, Precedence::Assign, node, scope, false);
    }

    return node;
  }

  return parseExpr(parser, Precedence::Assign, scope, false);
  // Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  // out->token = parser->cur_token;
  // out->location = parser->cur_token.location;
  // out->kind = NodeKind::Compound;
  // out->children.init(parser->allocator, 2);
  //
  // while (parser->cur_token.kind != TokenKind::BlockBegin) {
  //   Node *child;
  //
  //   if (parser->cur_token.kind == TokenKind::Name) {
  //     child = (Node *)parser->allocator->alloc(sizeof(Node));
  //     child->token = parser->cur_token;
  //     child->location = parser->cur_token.location;
  //     child->kind = NodeKind::Name;
  //     child->text = parser->cur_token.text;
  //
  //     expectEOF(parser->nextToken());
  //     if (parser->cur_token.kind == TokenKind::TypeSeperator) {
  //       child = parseField(parser, child, scope);
  //     } else {
  //       child = parseBinaryExpr(parser,
  //                                (Precedence)((int32_t)Precedence::Assign +
  //                                1), child, scope);
  //     }
  //   } else {
  //     child = parseExpr(parser, Precedence::Assign, scope);
  //   }
  //
  //   out->children.push(child);
  //
  //   if (parser->cur_token.kind != TokenKind::LineDelimiter) {
  //     break;
  //   }
  //   expectEOF(parser->nextToken());
  // }
  //
  // return out;
}

Node *parseAttribute(ASTParser *parser, Symbol *scope) {
  Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->kind = NodeKind::Attribute;
  out->children.init(parser->allocator, 2);

  expectEOF(parser->nextToken());
  expectToken(TokenKind::ScopeBegin);
  expectEOF(parser->nextToken());

  while (parser->cur_token.kind != TokenKind::ScopeEnd) {
    expectToken(TokenKind::Name);

    Node *attribute = (Node *)parser->allocator->alloc(sizeof(Node));
    attribute->token = parser->cur_token;
    attribute->location = parser->cur_token.location;
    attribute->kind = NodeKind::Member;
    attribute->member.name = parser->cur_token.text;
    attribute->member.value = nullptr;

    expectEOF(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::Eq) {
      expectEOF(parser->nextToken());
      attribute->member.value =
          parseExpr(parser, Precedence::Assign, scope, true);
    }

    out->children.push(attribute);

    if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
      break;
    }
    expectEOF(parser->nextToken());
  }

  expectToken(TokenKind::ScopeEnd);
  out->end_location = parser->cur_token.location;
  parser->nextToken();
  return out;
}

Node *parseCommentGroup(ASTParser *parser) {
  if (parser->cur_token.kind != TokenKind::Comment) {
    return nullptr;
  }

  Node *comment_group = (Node *)parser->allocator->alloc(sizeof(Node));
  comment_group->location = parser->cur_token.location;
  comment_group->kind = NodeKind::CommentGroup;
  comment_group->comment_group.init(parser->allocator, 2);

  while (parser->cur_token.kind == TokenKind::Comment) {
    comment_group->comment_group.push(parser->cur_token);
    expectEOF(parser->nextToken());
  }

  comment_group->end_location = parser->prev_token.location;
  return comment_group;
}

Node *parseStmt(ASTParser *parser, Symbol *scope) {
  Node *out = nullptr;

  Node *doc_comments = parseCommentGroup(parser);

  switch (parser->cur_token.kind) {
  case TokenKind::Name: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = out->token.location;
    out->kind = NodeKind::Name;
    out->text = out->token.text;

    expectEOF(parser->nextToken());
    if (parser->cur_token.kind == TokenKind::TypeSeperator) {
      out = parseField(parser, out, scope);
    } else {
      out = parseBinaryExpr(parser, Precedence::Assign, out, scope, true);

      if (parser->cur_token.kind == TokenKind::Eq ||
          parser->cur_token.kind == TokenKind::Assignment) {
        out = parseAssignExpr(parser, out, scope);
      }
    }
    break;
  }
  case TokenKind::ScopeBegin:
  case TokenKind::Operator: {
    out = parseExpr(parser, Precedence::Assign, scope, true);

    if (parser->cur_token.kind == TokenKind::Eq ||
        parser->cur_token.kind == TokenKind::Assignment) {
      out = parseAssignExpr(parser, out, scope);
    }
    break;
  }
  case TokenKind::Return: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Return;
    out->child = nullptr;

    expectEOF(parser->nextToken());
    if (parser->cur_token.kind != TokenKind::LineDelimiter) {
      out->child = parseExpr(parser, Precedence::Assign, scope, true);
    }
    break;
  }
  case TokenKind::If: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::If;

    // Create Scope
    Symbol *if_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    if_scope->init(parser->allocator, true, scope);
    if_scope->node = out;

    // Parse Conditional
    expectEOF(parser->nextToken());
    out->_if.conditional = parseConditional(parser, if_scope);
    expectToken(TokenKind::BlockBegin);

    // Parse Body
    out->_if.body = parseStmtCompound(parser, if_scope);
    out->_if._else = nullptr;

    // Parse Else and Else-If
    if (parser->cur_token.kind == TokenKind::Else) {
      expectEOF(parser->nextToken());

      if (parser->cur_token.kind == TokenKind::If) {
        out->_if._else = parseStmt(parser, scope);
      } else if (parser->cur_token.kind == TokenKind::BlockBegin) {
        // Create Scope
        Symbol *else_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
        else_scope->init(parser->allocator, true, scope);

        // Parse Body
        out->_if._else = parseStmtCompound(parser, else_scope);
        else_scope->node = out->_if._else;
      } else {
        std::cerr << "Else body must be either an if statement or compound\n";
        return nullptr;
      }
    }

    break;
  }
  case TokenKind::For: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::For;

    // Create Scope
    Symbol *for_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    for_scope->init(parser->allocator, true, scope);
    for_scope->node = out;

    // Parse Conditional
    expectEOF(parser->nextToken());
    out->_for.conditional = parseConditional(parser, for_scope);
    expectToken(TokenKind::BlockBegin);

    // Parse body
    out->_for.body = parseStmtCompound(parser, for_scope);
    break;
  }
  case TokenKind::Switch: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Switch;
    out->_switch.cases.init(parser->allocator, 8);

    expectEOF(parser->nextToken());
    out->_switch.conditional =
        parseExpr(parser, Precedence::Assign, scope, false);

    expectToken(TokenKind::BlockBegin);
    expectEOF(parser->nextToken());

    while (parser->cur_token.kind != TokenKind::BlockEnd) {
      Node *_case = (Node *)parser->allocator->alloc(sizeof(Node));
      _case->kind = NodeKind::Case;

      // Parse Constant
      _case->_case.constant =
          parseExpr(parser, Precedence::Assign, scope, true);
      expectToken(TokenKind::Case);
      _case->token = parser->cur_token;
      _case->location = parser->cur_token.location;

      expectEOF(parser->nextToken());

      // Create Scope
      Symbol *case_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
      case_scope->init(parser->allocator, true, scope);

      // Parse Body
      _case->_case.body = parseStmtCompound(parser, case_scope);
      out->_switch.cases.push(_case);
      case_scope->node = _case->_case.body;
    }

    expectToken(TokenKind::BlockEnd);
    parser->nextToken();

    break;
  }
  case TokenKind::Break: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Break;
    expectEOF(parser->nextToken());
    break;
  }
  case TokenKind::Continue: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Continue;
    expectEOF(parser->nextToken());
    break;
  }
  case TokenKind::Defer: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Defer;

    expectEOF(parser->nextToken());
    out->child = parseStmt(parser, scope);
    break;
  }
  case TokenKind::Comptime: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Comptime;

    expectEOF(parser->nextToken());
    out->child = parseStmt(parser, scope);
    break;
  }
  case TokenKind::Assembly: {
    out = (Node *)parser->allocator->alloc(sizeof(Node));
    out->token = parser->cur_token;
    out->location = parser->cur_token.location;
    out->kind = NodeKind::Assembly;
    out->assembly.instructions.init(parser->allocator, 8);

    expectEOF(parser->nextToken());
    expectToken(TokenKind::BlockBegin);
    expectEOF(parser->nextToken());

    // Parse Instructions
    while (parser->cur_token.kind != TokenKind::BlockEnd) {
      expectToken(TokenKind::Name);

      NodeAssembly::Instruction inst;
      inst.token = parser->cur_token;
      inst.location = parser->cur_token.location;
      inst.name = parser->cur_token.text;
      inst.arguments.init(parser->allocator, 4);

      // Parse Arguments
      expectEOF(parser->nextToken());
      while (parser->cur_token.kind != TokenKind::LineDelimiter) {
        NodeAssembly::Argument arg;
        arg.token = parser->cur_token;
        arg.location = parser->cur_token.location;
        arg.kind = NodeAssembly::Argument::Input;

        if (parser->cur_token.kind == TokenKind::Eq) {
          expectEOF(parser->nextToken());
          arg.kind = NodeAssembly::Argument::Return;
        }

        if (parser->cur_token.kind == TokenKind::Operator &&
            parser->cur_token._operator == Operator::Mod) {
          expectEOF(parser->nextToken());
          arg.kind = NodeAssembly::Argument::Register;
          arg.reg = parser->cur_token.text;
          expectEOF(parser->nextToken());
        } else {
          arg.node = parseExpr(parser, Precedence::Assign, scope, true);
        }

        inst.arguments.push(arg);
        if (parser->cur_token.kind != TokenKind::CommaDelimiter) {
          break;
        }
        expectEOF(parser->nextToken());
      }

      out->assembly.instructions.push(inst);
      expectToken(TokenKind::LineDelimiter);
      expectEOF(parser->nextToken());
    }

    expectToken(TokenKind::BlockEnd);
    expectEOF(parser->nextToken());
    break;
  }
  case TokenKind::BlockBegin: {
    // Create Scope
    Symbol *child_scope = (Symbol *)parser->allocator->alloc(sizeof(Symbol));
    child_scope->init(parser->allocator, true, scope);

    // Parse Body
    out = parseStmtCompound(parser, child_scope);
    child_scope->node = out;
    break;
  }
  case TokenKind::Attribute: {
    Node *attr = parseAttribute(parser, scope);
    expect(attr != nullptr, parser->cur_token.location,
           "Failed to parse attribute");

    out = parseStmt(parser, scope);
    expect(out != nullptr, parser->cur_token.location,
           "Failed to parse statement");

    if (out->kind == NodeKind::Field) {
      out->field.attributes = attr;
    } else {
      std::cout << "Cannot append attributes to `" << out->kind << "`\n";
    }

    break;
  }
  }

  if (out == nullptr) {
    return nullptr;
  }

  if (parser->cur_token.kind == TokenKind::LineDelimiter) {
    out->end_location = parser->cur_token.location;
    parser->nextToken();
  } else if (parser->prev_token.kind == TokenKind::LineDelimiter ||
             parser->prev_token.kind == TokenKind::BlockEnd) {
    out->end_location = parser->prev_token.location;
  } else {
    std::cerr << parser->cur_token.location
              << " Statement must end with either `;` or `}`\n";
    return nullptr;
  }

  // Comments
  out->doc_comments = doc_comments;
  if (parser->cur_token.kind == TokenKind::Comment &&
      parser->cur_token.location.line == out->end_location.line) {
    out->line_comments = parseCommentGroup(parser);
  }

  return out;
}

// The passed `scope` should be preallocated for this node
Node *parseStmtCompound(ASTParser *parser, Symbol *scope) {
  Node *out = (Node *)parser->allocator->alloc(sizeof(Node));
  out->kind = NodeKind::Compound;
  out->token = parser->cur_token;
  out->location = parser->cur_token.location;
  out->children.init(parser->allocator, 8);

  // Pass Marker
  if (parser->cur_token.kind == TokenKind::BlockBegin) {
    expectEOF(parser->nextToken());
  }

  // Parse Compound
  while (true) {
    Node *child = parseStmt(parser, scope);
    if (child == nullptr) {
      break;
    }

    out->children.push(child);
  }

  // Pass Marker
  out->end_location = parser->cur_token.location;
  if (parser->cur_token.kind == TokenKind::BlockEnd) {
    parser->nextToken();
  }

  return out;
}

void ASTParser::parse() {
  if (this->nextToken() == false) {
    this->ast = nullptr;
    return;
  }

  this->symbol = (Symbol *)this->allocator->alloc(sizeof(Symbol));
  this->symbol->parent = nullptr;
  this->symbol->location_aware = false;
  this->symbol->children.init(this->allocator, 8);

  // Setup symbol mangled name
  {
    std::string len_str = std::to_string(this->tokenizer.path.len);
    this->symbol->mangled_name.len = len_str.size() + this->tokenizer.path.len;
    this->symbol->mangled_name.ptr =
        (uint8_t *)allocator->alloc(this->symbol->mangled_name.len);
    memcpy(this->symbol->mangled_name.ptr, len_str.data(), len_str.size());
    memcpy(this->symbol->mangled_name.ptr + len_str.size(),
           this->tokenizer.path.ptr, this->tokenizer.path.len);
  }

  this->imports.init(this->allocator, 8);
  this->ast = parseStmtCompound(this, this->symbol);
  this->symbol->node = this->ast;
}

bool ASTParser::nextToken() {
  Token next_token = this->tokenizer.next();
  this->prev_token = this->cur_token;
  this->cur_token = next_token;
  return this->cur_token.kind != TokenKind::Eof;
}
