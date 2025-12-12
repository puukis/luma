#pragma once
#include "token.hpp"
#include <string>
#include <vector>

class Lexer {
public:
  explicit Lexer(std::string source);
  std::vector<Token> scanTokens();

private:
  std::string source_;
  std::vector<Token> tokens_;
  size_t start_ = 0;
  size_t current_ = 0;
  int line_ = 1;

  bool isAtEnd() const;

  char advance();
  char peek() const;
  char peekNext() const;
  bool match(char expected);

  void addToken(TokenType type);
  void scanToken();

  void stringLiteral();
  void numberLiteral();
  void identifierOrKeyword();

  static bool isAlpha(char c);
  static bool isDigit(char c);
  static bool isAlphaNumeric(char c);
};
