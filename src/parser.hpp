#pragma once
#include "ast.hpp"
#include "token.hpp"
#include <string>
#include <vector>

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);
  std::vector<StmtPtr> parse();

private:
  struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  std::vector<Token> tokens_;
  size_t current_ = 0;

  // statements
  StmtPtr declaration();
  StmtPtr classDeclaration();
  StmtPtr statement();
  StmtPtr printStatement();
  StmtPtr ifStatement();
  StmtPtr whileStatement();
  StmtPtr untilStatement();
  StmtPtr returnStatement();
  StmtPtr assignmentOrExprStatement();
  StmtPtr functionDeclaration();

  // Luma unique statements
  StmtPtr echoStatement();
  StmtPtr maybeStatement();

  std::unique_ptr<BlockStmt> block();

  // expressions
  ExprPtr expression();
  ExprPtr equality();
  ExprPtr comparison();
  ExprPtr term();
  ExprPtr factor();
  ExprPtr unary();
  ExprPtr call();
  ExprPtr primary();

  // helpers
  bool match(std::initializer_list<TokenType> types);
  bool check(TokenType type) const;
  const Token &advance();
  bool isAtEnd() const;
  const Token &peek() const;
  const Token &previous() const;

  const Token &consume(TokenType type, const std::string &message);
  ParseError error(const Token &token, const std::string &message);
  void synchronize();

  bool isStatementStart(TokenType t) const;
};
