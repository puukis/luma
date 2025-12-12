#pragma once
#include "ast.hpp"
#include <string>
#include <vector>

class AstPrinter {
public:
  std::string print(const std::vector<StmtPtr> &program);

private:
  int indent_ = 0;

  std::string stmtToString(const Stmt &s);
  std::string exprToString(const Expr &e);

  std::string ind() const;
};
