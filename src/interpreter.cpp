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
  auto previous = env_;
  env_ = std::move(newEnv);
  try {
    for (const auto &s : block.statements) {
      execute(*s);
    }
  } catch (...) {
    env_ = previous;
    throw;
  }
  env_ = previous;
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

Value Interpreter::callFunction(const FunctionPtr &fn,
                                const std::vector<Value> &args,
                                const Token &callSiteParen) {
  if (args.size() != fn->arity()) {
    throw std::runtime_error("Arity error at line " +
                             std::to_string(callSiteParen.line) +
                             ": function '" + fn->name.lexeme + "' expected " +
                             std::to_string(fn->arity()) + " args, got " +
                             std::to_string(args.size()));
  }

  auto callEnv = std::make_shared<Environment>(fn->closure);
  for (size_t i = 0; i < args.size(); i++) {
    callEnv->define(fn->params[i].lexeme, args[i]);
  }

  try {
    executeBlock(*fn->body, callEnv);
  } catch (const ReturnSignal &rs) {
    return rs.value;
  }

  return std::monostate{}; // nil if no return
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

    if (!std::holds_alternative<FunctionPtr>(callee)) {
      throw std::runtime_error("Type error at line " +
                               std::to_string(c->paren.line) +
                               ": can only call functions.");
    }

    return callFunction(std::get<FunctionPtr>(callee), args, c->paren);
  }

  if (auto *listExpr = dynamic_cast<const ListExpr *>(&expr)) {
    auto list = std::make_shared<List>();
    for (const auto &e : listExpr->elements) {
      list->elements.push_back(evaluate(*e));
    }
    return list;
  }

  if (auto *getExpr = dynamic_cast<const GetExpr *>(&expr)) {
    Value object = evaluate(*getExpr->object);
    Value index = evaluate(*getExpr->index);

    if (auto l = std::get_if<ListPtr>(&object)) {
      if (!std::holds_alternative<double>(index)) {
        throw std::runtime_error("List index must be a number.");
      }
      int idx = static_cast<int>(std::get<double>(index));
      auto &elems = (*l)->elements;
      if (idx < 0 || idx >= static_cast<int>(elems.size())) {
        throw std::runtime_error("List index out of bounds: " +
                                 std::to_string(idx));
      }
      return elems[idx];
    }

    throw std::runtime_error("Only lists support subscription.");
  }

  throw std::runtime_error("Unknown expression at runtime.");
}
