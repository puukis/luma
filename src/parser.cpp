#include "parser.hpp"
#include <sstream>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

std::vector<StmtPtr> Parser::parse() {
  std::vector<StmtPtr> stmts;
  while (!isAtEnd()) {
    try {
      stmts.push_back(declaration());
    } catch (const ParseError &) {
      synchronize();
    }
  }
  return stmts;
}

StmtPtr Parser::declaration() {
  if (match({TokenType::Def}))
    return functionDeclaration();
  return statement();
}

StmtPtr Parser::statement() {
  if (match({TokenType::Print}))
    return printStatement();
  if (match({TokenType::If}))
    return ifStatement();
  if (match({TokenType::While}))
    return whileStatement();
  if (match({TokenType::Until}))
    return untilStatement();
  if (match({TokenType::Return}))
    return returnStatement();
  if (match({TokenType::Echo}))
    return echoStatement();
  if (match({TokenType::Maybe}))
    return maybeStatement();
  return assignmentOrExprStatement();
}

// ... (skipping some methods) ...

StmtPtr Parser::untilStatement() {
  consume(TokenType::LeftParen, "Expected '(' after 'until'.");
  auto cond = expression();
  consume(TokenType::RightParen, "Expected ')' after until condition.");
  auto body = block();
  return std::make_unique<UntilStmt>(std::move(cond), std::move(body));
}

// ... (skipping to call()) ...

ExprPtr Parser::call() {
  auto expr = primary();

  while (true) {
    if (match({TokenType::LeftParen})) {
      std::vector<ExprPtr> args;
      if (!check(TokenType::RightParen)) {
        do {
          args.push_back(expression());
        } while (match({TokenType::Comma}));
      }
      Token paren =
          consume(TokenType::RightParen, "Expected ')' after arguments.");
      expr =
          std::make_unique<CallExpr>(std::move(expr), paren, std::move(args));
    } else if (match({TokenType::LeftBracket})) {
      Token bracket = previous();
      auto index = expression();
      consume(TokenType::RightBracket, "Expected ']' after index.");
      expr =
          std::make_unique<GetExpr>(std::move(expr), bracket, std::move(index));
    } else {
      break;
    }
  }

  return expr;
}

static std::string unquoteStringLexeme(const std::string &lexeme) {
  // lexeme includes quotes, e.g. "\"big\""
  if (lexeme.size() >= 2 && lexeme.front() == '"' && lexeme.back() == '"') {
    return lexeme.substr(1, lexeme.size() - 2);
  }
  return lexeme;
}

ExprPtr Parser::primary() {
  if (match({TokenType::Number})) {
    const auto &t = previous();
    double v = 0.0;
    try {
      v = std::stod(t.lexeme);
    } catch (...) {
      throw error(t, "Invalid number literal: " + t.lexeme);
    }
    return LiteralExpr::number(v);
  }

  if (match({TokenType::String})) {
    const auto &t = previous();
    return LiteralExpr::str(unquoteStringLexeme(t.lexeme));
  }

  if (match({TokenType::True}))
    return LiteralExpr::boolean(true);
  if (match({TokenType::False}))
    return LiteralExpr::boolean(false);
  if (match({TokenType::Nil}))
    return LiteralExpr::nil();

  if (match({TokenType::Identifier})) {
    return std::make_unique<VariableExpr>(previous());
  }

  // Bracket for List Literals: [1, 2, 3]
  if (match({TokenType::LeftBracket})) {
    std::vector<ExprPtr> elements;
    if (!check(TokenType::RightBracket)) {
      do {
        elements.push_back(expression());
      } while (match({TokenType::Comma}));
    }
    consume(TokenType::RightBracket, "Expected ']' after list elements.");
    return std::make_unique<ListExpr>(std::move(elements));
  }

  if (match({TokenType::LeftParen})) {
    auto expr = expression();
    consume(TokenType::RightParen, "Expected ')' after expression.");
    return std::make_unique<GroupingExpr>(std::move(expr));
  }

  throw error(peek(), "Expected expression.");
}

StmtPtr Parser::functionDeclaration() {
  Token name =
      consume(TokenType::Identifier, "Expected function name after 'def'.");
  consume(TokenType::LeftParen, "Expected '(' after function name.");

  std::vector<Token> params;
  if (!check(TokenType::RightParen)) {
    do {
      params.push_back(
          consume(TokenType::Identifier, "Expected parameter name."));
    } while (match({TokenType::Comma}));
  }

  consume(TokenType::RightParen, "Expected ')' after parameters.");
  auto body = block();
  return std::make_unique<FuncDefStmt>(name, std::move(params),
                                       std::move(body));
}

StmtPtr Parser::printStatement() {
  consume(TokenType::LeftParen, "Expected '(' after 'print'.");
  auto value = expression();
  consume(TokenType::RightParen, "Expected ')' after print expression.");
  match({TokenType::Semicolon});
  return std::make_unique<PrintStmt>(std::move(value));
}

StmtPtr Parser::ifStatement() {
  consume(TokenType::LeftParen, "Expected '(' after 'if'.");
  auto cond = expression();
  consume(TokenType::RightParen, "Expected ')' after if condition.");

  auto thenBlock = block();

  StmtPtr elseBranch = nullptr;
  if (match({TokenType::Else})) {
    if (match({TokenType::If})) {
      // else if: recursively parse another if statement
      elseBranch = ifStatement();
    } else {
      // else: parse a block
      elseBranch = block();
    }
  }

  return std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock),
                                  std::move(elseBranch));
}

StmtPtr Parser::whileStatement() {
  consume(TokenType::LeftParen, "Expected '(' after 'while'.");
  auto cond = expression();
  consume(TokenType::RightParen, "Expected ')' after while condition.");
  auto body = block();
  return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

StmtPtr Parser::returnStatement() {
  Token kw = previous();
  ExprPtr value = nullptr;

  // return; OR return } OR return <expr>
  if (!check(TokenType::Semicolon) && !check(TokenType::RightBrace) &&
      !isAtEnd()) {
    value = expression();
  }
  match({TokenType::Semicolon});
  return std::make_unique<ReturnStmt>(kw, std::move(value));
}

// ========== Luma Unique Statements ==========

StmtPtr Parser::echoStatement() {
  // echo <count> { ... }
  auto count = expression();
  auto body = block();
  return std::make_unique<EchoStmt>(std::move(count), std::move(body));
}

StmtPtr Parser::maybeStatement() {
  // maybe { ... } otherwise { ... }
  auto tryBlock = block();

  std::unique_ptr<BlockStmt> otherwiseBlock = nullptr;
  if (match({TokenType::Otherwise})) {
    otherwiseBlock = block();
  }

  return std::make_unique<MaybeStmt>(std::move(tryBlock),
                                     std::move(otherwiseBlock));
}

StmtPtr Parser::assignmentOrExprStatement() {
  // Lookahead for: IDENT '=' ... OR IDENT '<->' IDENT
  if (check(TokenType::Identifier)) {
    // Check for swap: IDENT <-> IDENT
    if (current_ + 1 < tokens_.size() &&
        tokens_[current_ + 1].type == TokenType::Swap) {
      Token left = advance(); // first IDENT
      advance();              // '<->'
      Token right = consume(TokenType::Identifier,
                            "Expected identifier after '<->' swap operator.");
      match({TokenType::Semicolon});
      return std::make_unique<SwapStmt>(left, right);
    }
    // Check for assignment: IDENT = expr
    if (current_ + 1 < tokens_.size() &&
        tokens_[current_ + 1].type == TokenType::Equal) {
      Token name = advance(); // IDENT
      advance();              // '='
      auto value = expression();
      match({TokenType::Semicolon});
      return std::make_unique<VarAssignStmt>(name, std::move(value));
    }
  }

  auto expr = expression();
  match({TokenType::Semicolon});
  return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<BlockStmt> Parser::block() {
  consume(TokenType::LeftBrace, "Expected '{' to start block.");
  std::vector<StmtPtr> stmts;

  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    stmts.push_back(declaration());
  }

  consume(TokenType::RightBrace, "Expected '}' after block.");
  return std::make_unique<BlockStmt>(std::move(stmts));
}

// -------------------- expressions --------------------

ExprPtr Parser::expression() { return equality(); }

ExprPtr Parser::equality() {
  auto expr = comparison();
  while (match({TokenType::BangEqual, TokenType::EqualEqual})) {
    Token op = previous();
    auto right = comparison();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::comparison() {
  auto expr = term();
  while (match({TokenType::Greater, TokenType::GreaterEqual, TokenType::Less,
                TokenType::LessEqual})) {
    Token op = previous();
    auto right = term();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::term() {
  auto expr = factor();
  while (match({TokenType::Plus, TokenType::Minus})) {
    Token op = previous();
    auto right = factor();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::factor() {
  auto expr = unary();
  while (match({TokenType::Star, TokenType::Slash})) {
    Token op = previous();
    auto right = unary();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::unary() {
  if (match({TokenType::Bang, TokenType::Minus})) {
    Token op = previous();
    auto right = unary();
    return std::make_unique<UnaryExpr>(op, std::move(right));
  }
  return call();
}

// -------------------- helpers --------------------

bool Parser::match(std::initializer_list<TokenType> types) {
  for (auto t : types) {
    if (check(t)) {
      advance();
      return true;
    }
  }
  return false;
}

bool Parser::check(TokenType type) const {
  if (isAtEnd())
    return false;
  return peek().type == type;
}

const Token &Parser::advance() {
  if (!isAtEnd())
    current_++;
  return previous();
}

bool Parser::isAtEnd() const { return peek().type == TokenType::Eof; }

const Token &Parser::peek() const { return tokens_[current_]; }

const Token &Parser::previous() const { return tokens_[current_ - 1]; }

const Token &Parser::consume(TokenType type, const std::string &message) {
  if (check(type))
    return advance();
  throw error(peek(), message);
}

Parser::ParseError Parser::error(const Token &token,
                                 const std::string &message) {
  std::ostringstream oss;
  oss << "Parse error at line " << token.line << ": " << message << " (got '"
      << token.lexeme << "')";
  return ParseError(oss.str());
}

void Parser::synchronize() {
  advance();

  while (!isAtEnd()) {
    if (previous().type == TokenType::Semicolon)
      return;

    switch (peek().type) {
    case TokenType::Def:
    case TokenType::If:
    case TokenType::While:
    case TokenType::Return:
    case TokenType::Print:
    case TokenType::Else:
      return;
    default:
      break;
    }
    advance();
  }
}
