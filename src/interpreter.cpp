#include "interpreter.hpp"
#include <iostream>
#include <stdexcept>

Interpreter::Interpreter() {
  globals_ = std::make_shared<Environment>();
  env_ = globals_;
}

void Interpreter::run(const std::vector<StmtPtr> &program) {
  try {
    for (const auto &s : program) {
      execute(*s);
    }
  } catch (const ReturnSignal &) {
    throw std::runtime_error("Return used outside of a function.");
  }
}

void Interpreter::assignOrDefine(const Token &name, Value value) {
  if (env_->has(name.lexeme)) {
    env_->assign(name, std::move(value));
  } else {
    env_->define(name.lexeme, std::move(value));
  }
}

void Interpreter::executeBlock(const BlockStmt &block,
                               std::shared_ptr<Environment> newEnv) {
  std::shared_ptr<Environment> previous = env_;
  try {
    env_ = newEnv;
    for (const auto &stmt : block.statements) {
      execute(*stmt);
    }
  } catch (...) {
    env_ = previous;
    throw;
  }
  env_ = previous;
}

void Interpreter::visitClassStmt(const ClassStmt &stmt) {
  env_->define(stmt.name.lexeme, std::monostate{});

  std::map<std::string, FunctionPtr> methods;
  for (const auto &method : stmt.methods) {
    auto func = std::make_shared<Function>();
    func->name = method->name;
    func->params = method->params;
    func->body = method->body.get();
    func->closure = env_; // Closure captures defining scope
    methods[method->name.lexeme] = func;
  }

  auto klass = std::make_shared<LumaClass>();
  klass->name = stmt.name.lexeme;
  klass->methods = std::move(methods);

  env_->assign(stmt.name, klass);
}

void Interpreter::execute(const Stmt &stmt) {
  if (auto *x = dynamic_cast<const ExprStmt *>(&stmt)) {
    (void)evaluate(*x->expr);
    return;
  }

  if (auto *p = dynamic_cast<const PrintStmt *>(&stmt)) {
    Value v = evaluate(*p->expr);
    std::cout << valueToString(v) << "\n";
    return;
  }

  if (auto *a = dynamic_cast<const VarAssignStmt *>(&stmt)) {
    Value v = evaluate(*a->value);
    assignOrDefine(a->name, std::move(v));
    return;
  }

  if (auto *b = dynamic_cast<const BlockStmt *>(&stmt)) {
    auto newEnv = std::make_shared<Environment>(env_);
    executeBlock(*b, std::move(newEnv));
    return;
  }

  if (auto *i = dynamic_cast<const IfStmt *>(&stmt)) {
    Value cond = evaluate(*i->condition);
    if (isTruthy(cond)) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*i->thenBranch, std::move(newEnv));
    } else if (i->elseBranch) {
      // elseBranch can be a BlockStmt or another IfStmt (else if)
      if (auto *elseBlock =
              dynamic_cast<const BlockStmt *>(i->elseBranch.get())) {
        auto newEnv = std::make_shared<Environment>(env_);
        executeBlock(*elseBlock, std::move(newEnv));
      } else {
        // It's another IfStmt (else if chain)
        execute(*i->elseBranch);
      }
    }
    return;
  }

  if (auto *w = dynamic_cast<const WhileStmt *>(&stmt)) {
    while (isTruthy(evaluate(*w->condition))) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*w->body, std::move(newEnv));
    }
    return;
  }

  if (auto *u = dynamic_cast<const UntilStmt *>(&stmt)) {
    // Until = while (!condition)
    while (!isTruthy(evaluate(*u->condition))) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*u->body, std::move(newEnv));
    }
    return;
  }

  if (auto *r = dynamic_cast<const ReturnStmt *>(&stmt)) {
    Value v = std::monostate{};
    if (r->value)
      v = evaluate(*r->value);
    throw ReturnSignal{std::move(v)};
  }

  if (auto *f = dynamic_cast<const FuncDefStmt *>(&stmt)) {
    auto fn = std::make_shared<Function>();
    fn->name = f->name;
    fn->params = f->params;
    fn->body = f->body.get();
    fn->closure = env_; // capture defining environment
    env_->define(f->name.lexeme, fn);
    return;
  }

  if (auto *c = dynamic_cast<const ClassStmt *>(&stmt)) {
    visitClassStmt(*c);
    return;
  }

  // ========== Luma Unique Statements ==========

  // echo N { ... } - repeat block N times
  if (auto *e = dynamic_cast<const EchoStmt *>(&stmt)) {
    Value countVal = evaluate(*e->count);
    if (!std::holds_alternative<double>(countVal)) {
      throw std::runtime_error("Echo count must be a number.");
    }
    int count = static_cast<int>(std::get<double>(countVal));
    if (count < 0) {
      throw std::runtime_error("Echo count cannot be negative.");
    }
    for (int i = 0; i < count; i++) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*e->body, std::move(newEnv));
    }
    return;
  }

  // a <-> b - swap two variables
  if (auto *s = dynamic_cast<const SwapStmt *>(&stmt)) {
    Value leftVal = env_->get(s->left);
    Value rightVal = env_->get(s->right);
    env_->assign(s->left, std::move(rightVal));
    env_->assign(s->right, std::move(leftVal));
    return;
  }

  // maybe { ... } otherwise { ... } - error handling
  if (auto *m = dynamic_cast<const MaybeStmt *>(&stmt)) {
    try {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*m->tryBlock, std::move(newEnv));
    } catch (const std::runtime_error &) {
      // If an error occurred and we have an otherwise block, execute it
      if (m->otherwiseBlock) {
        auto newEnv = std::make_shared<Environment>(env_);
        executeBlock(*m->otherwiseBlock, std::move(newEnv));
      }
      // If no otherwise block, silently continue (that's the "maybe"
      // philosophy)
    }
    return;
  }

  throw std::runtime_error("Unknown statement at runtime.");
}

static void requireNumber(const Value &v, const std::string &where) {
  if (!std::holds_alternative<double>(v)) {
    throw std::runtime_error("Type error: expected number in " + where +
                             ", got " + valueToString(v));
  }
}

Value Interpreter::callFunction(const Value &callee,
                                const std::vector<Value> &args,
                                const Token &callSiteParen) {
  if (auto fn = std::get_if<FunctionPtr>(&callee)) {
    FunctionPtr function = *fn;
    if (args.size() != function->arity()) {
      throw std::runtime_error("Expected " + std::to_string(function->arity()) +
                               " arguments but got " +
                               std::to_string(args.size()) + ".");
    }

    auto environment = std::make_shared<Environment>(function->closure);
    for (size_t i = 0; i < function->params.size(); ++i) {
      environment->define(function->params[i].lexeme, args[i]);
    }

    try {
      executeBlock(*function->body, environment);
    } catch (const ReturnSignal &returnValue) {
      return returnValue.value;
    }
    return std::monostate{};
  }

  if (auto klass = std::get_if<ClassPtr>(&callee)) {
    // Create instance
    auto instance = std::make_shared<LumaInstance>(*klass);

    // Look for definition of "init"
    FunctionPtr init = (*klass)->findMethod("init");
    if (init) {
      // Bind 'this' to init
      // We need to bind 'this' to the method call.
      // Current implementation of Function doesn't support 'this' binding
      // implicitly? We need a way to bind 'this'. Standard Lox way:
      // Function.bind(instance) -> new Function with environment containing
      // "this".

      auto environment = std::make_shared<Environment>(init->closure);
      environment->define("this", instance); // Bind 'this'

      // Handle args
      if (args.size() != init->arity()) {
        throw std::runtime_error("Expected " + std::to_string(init->arity()) +
                                 " arguments but got " +
                                 std::to_string(args.size()) + ".");
      }
      for (size_t i = 0; i < init->params.size(); ++i) {
        environment->define(init->params[i].lexeme, args[i]);
      }

      try {
        executeBlock(*init->body, environment);
      } catch (const ReturnSignal &returnValue) {
        // initializer usually returns 'this', but if user returns something
        // else? Lox: always return this. Luma: let's return this implicitly
        // unless explicit return? For now, ignore return value of init and
        // return instance? Or allow explicit return? Lox ignores explicit
        // return in init. Let's mimic Lox: init returns 'this'.
      }
    } else {
      if (!args.empty()) {
        throw std::runtime_error("Expected 0 arguments but got " +
                                 std::to_string(args.size()) + ".");
      }
    }

    return instance;
  }

  throw std::runtime_error("Can only call functions and classes.");
}
Value Interpreter::evaluate(const Expr &expr) {
  if (auto *l = dynamic_cast<const LiteralExpr *>(&expr)) {
    switch (l->kind) {
    case LiteralExpr::Kind::Number:
      return l->numberValue;
    case LiteralExpr::Kind::String:
      return l->stringValue;
    case LiteralExpr::Kind::Bool:
      return l->boolValue;
    case LiteralExpr::Kind::Nil:
      return std::monostate{};
    }
  }

  if (auto *v = dynamic_cast<const VariableExpr *>(&expr)) {
    return env_->get(v->name);
  }

  if (auto *g = dynamic_cast<const GroupingExpr *>(&expr)) {
    return evaluate(*g->expr);
  }

  if (auto *u = dynamic_cast<const UnaryExpr *>(&expr)) {
    Value right = evaluate(*u->right);
    if (u->op.type == TokenType::Minus) {
      requireNumber(right, "unary '-'");
      return -std::get<double>(right);
    }
    if (u->op.type == TokenType::Bang) {
      return !isTruthy(right);
    }
    throw std::runtime_error("Unknown unary operator '" + u->op.lexeme + "'");
  }

  if (auto *b = dynamic_cast<const BinaryExpr *>(&expr)) {
    Value left = evaluate(*b->left);
    Value right = evaluate(*b->right);

    switch (b->op.type) {
    case TokenType::Plus:
      if (std::holds_alternative<double>(left) &&
          std::holds_alternative<double>(right)) {
        return std::get<double>(left) + std::get<double>(right);
      }
      if (std::holds_alternative<std::string>(left) &&
          std::holds_alternative<std::string>(right)) {
        return std::get<std::string>(left) + std::get<std::string>(right);
      }
      throw std::runtime_error(
          "Type error: '+' needs (number,number) or (string,string).");
    case TokenType::Minus:
      requireNumber(left, "binary '-'");
      requireNumber(right, "binary '-'");
      return std::get<double>(left) - std::get<double>(right);
    case TokenType::Star:
      requireNumber(left, "binary '*'");
      requireNumber(right, "binary '*'");
      return std::get<double>(left) * std::get<double>(right);
    case TokenType::Slash:
      requireNumber(left, "binary '/'");
      requireNumber(right, "binary '/'");
      if (std::get<double>(right) == 0.0)
        throw std::runtime_error("Runtime error: division by zero.");
      return std::get<double>(left) / std::get<double>(right);
    case TokenType::Greater:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) > std::get<double>(right);
    case TokenType::GreaterEqual:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) >= std::get<double>(right);
    case TokenType::Less:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) < std::get<double>(right);
    case TokenType::LessEqual:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) <= std::get<double>(right);
    case TokenType::EqualEqual:
      return valuesEqual(left, right);
    case TokenType::BangEqual:
      return !valuesEqual(left, right);
    default:
      throw std::runtime_error("Unknown binary operator '" + b->op.lexeme +
                               "'");
    }
  }

  if (auto *c = dynamic_cast<const CallExpr *>(&expr)) {
    Value callee = evaluate(*c->callee);
    std::vector<Value> args;
    args.reserve(c->args.size());
    for (const auto &a : c->args)
      args.push_back(evaluate(*a));

    if (std::holds_alternative<FunctionPtr>(callee) ||
        std::holds_alternative<ClassPtr>(callee)) {
      return callFunction(callee, args, c->paren);
    }
    throw std::runtime_error("Can only call functions and classes.");
  }

  if (auto *indexExpr = dynamic_cast<const IndexExpr *>(&expr)) {
    Value object = evaluate(*indexExpr->object);
    Value index = evaluate(*indexExpr->index);

    if (auto listPtr = std::get_if<ListPtr>(&object)) {
      if (!std::holds_alternative<double>(index)) {
        throw std::runtime_error("List index must be a number.");
      }
      int idx = static_cast<int>(std::get<double>(index));
      auto &elements = (*listPtr)->elements;
      if (idx < 0 || idx >= static_cast<int>(elements.size())) {
        throw std::runtime_error("List index out of bounds.");
      }
      return elements[idx];
    }

    if (auto mapPtr = std::get_if<MapPtr>(&object)) {
      if (auto s = std::get_if<std::string>(&index)) {
        auto &values = (*mapPtr)->values;
        if (values.count(*s)) {
          return values.at(*s);
        }
        throw std::runtime_error("Undefined key '" + *s + "'.");
      }
      throw std::runtime_error("Map key must be a string.");
    }
    throw std::runtime_error("Only lists and maps support subscription.");
  }

  if (auto *indexSetExpr = dynamic_cast<const IndexSetExpr *>(&expr)) {
    Value object = evaluate(*indexSetExpr->object);
    Value index = evaluate(*indexSetExpr->index);
    Value value = evaluate(*indexSetExpr->value);

    if (auto listPtr = std::get_if<ListPtr>(&object)) {
      if (!std::holds_alternative<double>(index)) {
        throw std::runtime_error("List index must be a number.");
      }
      int idx = static_cast<int>(std::get<double>(index));
      auto &elements = (*listPtr)->elements;
      if (idx < 0 || idx >= static_cast<int>(elements.size())) {
        throw std::runtime_error("List index out of bounds.");
      }
      elements[idx] = value;
      return value;
    }

    if (auto mapPtr = std::get_if<MapPtr>(&object)) {
      if (auto s = std::get_if<std::string>(&index)) {
        (*mapPtr)->values[*s] = value;
        return value;
      }
      throw std::runtime_error("Map key must be a string.");
    }
    throw std::runtime_error("Only lists and maps support assignment.");
  }

  if (auto *listExpr = dynamic_cast<const ListExpr *>(&expr)) {
    auto list = std::make_shared<List>();
    for (const auto &e : listExpr->elements) {
      list->elements.push_back(evaluate(*e));
    }
    return list;
  }

  if (auto get = dynamic_cast<const GetExpr *>(&expr)) {
    Value object = evaluate(*get->object);
    if (auto inst = std::get_if<InstancePtr>(&object)) {
      LumaInstance *instance = inst->get();
      if (instance->fields.count(get->name.lexeme)) {
        return instance->fields.at(get->name.lexeme);
      }
      FunctionPtr method = instance->klass->findMethod(get->name.lexeme);
      if (method) {
        auto newEnv = std::make_shared<Environment>(method->closure);
        newEnv->define("this", *inst);
        auto boundMethod = std::make_shared<Function>(*method);
        boundMethod->closure = newEnv;
        return boundMethod;
      }
      throw std::runtime_error("Undefined property '" + get->name.lexeme +
                               "'.");
    }
    throw std::runtime_error("Only instances have properties.");
  }

  if (auto set = dynamic_cast<const SetExpr *>(&expr)) {
    Value object = evaluate(*set->object);
    if (auto inst = std::get_if<InstancePtr>(&object)) {
      Value value = evaluate(*set->value);
      (*inst)->fields[set->name.lexeme] = value;
      return value;
    }
    throw std::runtime_error("Only instances have properties.");
  }

  if (auto th = dynamic_cast<const ThisExpr *>(&expr)) {
    return env_->get(th->keyword);
  }

  if (auto mp = dynamic_cast<const MapExpr *>(&expr)) {
    auto map = std::make_shared<LumaMap>();
    for (size_t i = 0; i < mp->keys.size(); ++i) {
      Value k = evaluate(*mp->keys[i]);
      Value v = evaluate(*mp->values[i]);
      if (auto s = std::get_if<std::string>(&k)) {
        map->values[*s] = v;
      } else {
        throw std::runtime_error("Map keys must be strings.");
      }
    }
    return map;
  }

  throw std::runtime_error("Unknown expression type.");
}
