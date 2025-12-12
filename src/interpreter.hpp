#pragma once
#include <memory>
#include <vector>

#include "ast.hpp"
#include "environment.hpp"
#include "value.hpp"

class Interpreter {
public:
  Interpreter();

  void run(const std::vector<StmtPtr> &program);

private:
  std::shared_ptr<Environment> globals_;
  std::shared_ptr<Environment> env_;

  // Return signal used internally
  struct ReturnSignal {
    Value value;
  };

  void execute(const Stmt &stmt);
  Value evaluate(const Expr &expr);

  void executeBlock(const BlockStmt &block,
                    std::shared_ptr<Environment> newEnv);

  // helpers
  void assignOrDefine(const Token &name, Value value);

  Value callFunction(const FunctionPtr &fn, const std::vector<Value> &args,
                     const Token &callSiteParen);
};
