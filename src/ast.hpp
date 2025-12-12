#pragma once
#include "token.hpp"
#include <memory>
#include <string>
#include <vector>

// ---------- Expressions ----------
struct Expr {
  virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr : Expr {
  enum class Kind { Number, String, Bool, Nil };

  Kind kind;
  double numberValue = 0.0;
  std::string stringValue;
  bool boolValue = false;

  static ExprPtr number(double v) {
    auto e = std::make_unique<LiteralExpr>();
    e->kind = Kind::Number;
    e->numberValue = v;
    return e;
  }
  static ExprPtr str(std::string v) {
    auto e = std::make_unique<LiteralExpr>();
    e->kind = Kind::String;
    e->stringValue = std::move(v);
    return e;
  }
  static ExprPtr boolean(bool v) {
    auto e = std::make_unique<LiteralExpr>();
    e->kind = Kind::Bool;
    e->boolValue = v;
    return e;
  }
  static ExprPtr nil() {
    auto e = std::make_unique<LiteralExpr>();
    e->kind = Kind::Nil;
    return e;
  }
};

struct VariableExpr : Expr {
  Token name;
  explicit VariableExpr(Token n) : name(std::move(n)) {}
};

struct GroupingExpr : Expr {
  ExprPtr expr;
  explicit GroupingExpr(ExprPtr e) : expr(std::move(e)) {}
};

struct UnaryExpr : Expr {
  Token op;
  ExprPtr right;
  UnaryExpr(Token o, ExprPtr r) : op(std::move(o)), right(std::move(r)) {}
};

struct BinaryExpr : Expr {
  ExprPtr left;
  Token op;
  ExprPtr right;
  BinaryExpr(ExprPtr l, Token o, ExprPtr r)
      : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
};

struct CallExpr : Expr {
  ExprPtr callee;
  Token paren; // the ')'
  std::vector<ExprPtr> args;
  CallExpr(ExprPtr c, Token p, std::vector<ExprPtr> a)
      : callee(std::move(c)), paren(std::move(p)), args(std::move(a)) {}
};

struct ListExpr : Expr {
  std::vector<ExprPtr> elements;
  explicit ListExpr(std::vector<ExprPtr> e) : elements(std::move(e)) {}
};

struct GetExpr : Expr {
  ExprPtr object;
  Token name;
  explicit GetExpr(ExprPtr o, Token n)
      : object(std::move(o)), name(std::move(n)) {}
};

struct IndexExpr : Expr {
  ExprPtr object;
  Token bracket;
  ExprPtr index;
  IndexExpr(ExprPtr o, Token b, ExprPtr i)
      : object(std::move(o)), bracket(std::move(b)), index(std::move(i)) {}
};

struct IndexSetExpr : Expr {
  ExprPtr object;
  Token bracket;
  ExprPtr index;
  ExprPtr value;
  IndexSetExpr(ExprPtr o, Token b, ExprPtr i, ExprPtr v)
      : object(std::move(o)), bracket(std::move(b)), index(std::move(i)),
        value(std::move(v)) {}
};

struct SetExpr : Expr {
  ExprPtr object;
  Token name;
  ExprPtr value;
  SetExpr(ExprPtr o, Token n, ExprPtr v)
      : object(std::move(o)), name(std::move(n)), value(std::move(v)) {}
};

struct ThisExpr : Expr {
  Token keyword;
  explicit ThisExpr(Token k) : keyword(std::move(k)) {}
};

struct MapExpr : Expr {
  std::vector<ExprPtr> keys;
  std::vector<ExprPtr> values;
  MapExpr(std::vector<ExprPtr> k, std::vector<ExprPtr> v)
      : keys(std::move(k)), values(std::move(v)) {}
};

// ---------- Statements ----------
struct Stmt {
  virtual ~Stmt() = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt : Stmt {
  ExprPtr expr;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct PrintStmt : Stmt {
  ExprPtr expr;
  explicit PrintStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct VarAssignStmt : Stmt {
  Token name;
  ExprPtr value;
  VarAssignStmt(Token n, ExprPtr v) : name(std::move(n)), value(std::move(v)) {}
};

struct BlockStmt : Stmt {
  std::vector<StmtPtr> statements;
  explicit BlockStmt(std::vector<StmtPtr> s) : statements(std::move(s)) {}
};

struct IfStmt : Stmt {
  ExprPtr condition;
  std::unique_ptr<BlockStmt> thenBranch;
  StmtPtr
      elseBranch; // may be null, can be BlockStmt or another IfStmt (else if)
  IfStmt(ExprPtr cond, std::unique_ptr<BlockStmt> t, StmtPtr e)
      : condition(std::move(cond)), thenBranch(std::move(t)),
        elseBranch(std::move(e)) {}
};

struct WhileStmt : Stmt {
  ExprPtr condition;
  std::unique_ptr<BlockStmt> body;
  WhileStmt(ExprPtr cond, std::unique_ptr<BlockStmt> b)
      : condition(std::move(cond)), body(std::move(b)) {}
};

struct UntilStmt : Stmt {
  ExprPtr condition;
  std::unique_ptr<BlockStmt> body;
  UntilStmt(ExprPtr cond, std::unique_ptr<BlockStmt> b)
      : condition(std::move(cond)), body(std::move(b)) {}
};

struct ReturnStmt : Stmt {
  Token keyword; // 'return'
  ExprPtr value; // may be null
  ReturnStmt(Token k, ExprPtr v) : keyword(std::move(k)), value(std::move(v)) {}
};

struct FuncDefStmt : Stmt {
  Token name;
  std::vector<Token> params;
  std::unique_ptr<BlockStmt> body;
  FuncDefStmt(Token n, std::vector<Token> p, std::unique_ptr<BlockStmt> b)
      : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
};

struct ClassStmt : Stmt {
  Token name;
  std::vector<std::shared_ptr<FuncDefStmt>> methods;
  ClassStmt(Token n, std::vector<std::shared_ptr<FuncDefStmt>> m)
      : name(std::move(n)), methods(std::move(m)) {}
};

// ========== Luma Unique Statements ==========

// echo N { ... } - repeat block N times
struct EchoStmt : Stmt {
  ExprPtr count;
  std::unique_ptr<BlockStmt> body;
  EchoStmt(ExprPtr c, std::unique_ptr<BlockStmt> b)
      : count(std::move(c)), body(std::move(b)) {}
};

// a <-> b - swap two variables
struct SwapStmt : Stmt {
  Token left;
  Token right;
  SwapStmt(Token l, Token r) : left(std::move(l)), right(std::move(r)) {}
};

// maybe { ... } otherwise { ... } - error handling
struct MaybeStmt : Stmt {
  std::unique_ptr<BlockStmt> tryBlock;
  std::unique_ptr<BlockStmt> otherwiseBlock; // may be null
  MaybeStmt(std::unique_ptr<BlockStmt> t, std::unique_ptr<BlockStmt> o)
      : tryBlock(std::move(t)), otherwiseBlock(std::move(o)) {}
};
