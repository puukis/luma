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
  // Module system: module declaration must come first
  if (match({TokenType::Module}))
    return moduleDeclaration();
  if (match({TokenType::Use}))
    return useStatement();

  // Visibility modifiers
  Visibility vis = Visibility::Closed;
  if (match({TokenType::Open})) {
    vis = Visibility::Open;
  } else if (match({TokenType::Closed})) {
    vis = Visibility::Closed;
  }

  if (match({TokenType::Def}))
    return functionDeclaration(vis);
  if (match({TokenType::Class}))
    return classDeclaration(vis);

  // If we had a visibility modifier but no def/class, error
  if (vis == Visibility::Open) {
    throw error(previous(), "'open' must be followed by 'def' or 'class'.");
  }

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

// ========== Module System Parsing ==========

std::vector<Token> Parser::parseModuleId() {
  // Parse: @ident(.ident)*
  std::vector<Token> parts;

  // Expect @ token (already consumed by caller usually, but we store it)
  Token atToken = previous(); // The '@' was matched by caller
  parts.push_back(atToken);

  // First identifier segment
  Token ident = consume(TokenType::Identifier, "Expected module name after '@'.");
  parts.push_back(ident);

  // Optional: .ident segments
  while (match({TokenType::Dot})) {
    parts.push_back(previous()); // the '.'
    Token seg = consume(TokenType::Identifier, "Expected identifier after '.' in module ID.");
    parts.push_back(seg);
  }

  return parts;
}

StmtPtr Parser::moduleDeclaration() {
  // 'module' was already consumed
  consume(TokenType::At, "Expected '@' after 'module'.");
  auto parts = parseModuleId();
  match({TokenType::Semicolon}); // optional semicolon
  return std::make_unique<ModuleStmt>(std::move(parts));
}

StmtPtr Parser::useStatement() {
  // 'use' was already consumed
  consume(TokenType::At, "Expected '@' after 'use'.");
  auto parts = parseModuleId();
  consume(TokenType::As, "Expected 'as' after module ID in use statement.");
  Token alias = consume(TokenType::Identifier, "Expected alias name after 'as'.");
  match({TokenType::Semicolon}); // optional semicolon
  return std::make_unique<UseStmt>(std::move(parts), std::move(alias));
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
  ExprPtr expr = primary();

  while (true) {
    if (match({TokenType::LeftParen})) {
      std::vector<ExprPtr> args;
      Token paren = previous(); // use '(' as location
      if (!check(TokenType::RightParen)) {
        do {
          args.push_back(expression());
        } while (match({TokenType::Comma}));
      }
      consume(TokenType::RightParen, "Expected ')' after arguments.");
      expr = std::make_unique<CallExpr>(std::move(expr), std::move(paren),
                                        std::move(args));
    } else if (match({TokenType::Dot})) {
      Token name =
          consume(TokenType::Identifier, "Expected property name after '.'.");
      expr = std::make_unique<GetExpr>(std::move(expr), std::move(name));
    } else if (match({TokenType::LeftBracket})) {
      Token bracket = previous();
      auto index = expression();
      consume(TokenType::RightBracket, "Expected ']' after index.");
      expr = std::make_unique<IndexExpr>(std::move(expr), std::move(bracket),
                                         std::move(index));
    } else {
      break;
    }
  }

  return expr;
}

static std::string unquoteStringLexeme(const std::string &lexeme) {
  // lexeme includes quotes, e.g. "\"big\""
  if (lexeme.size() >= 2 && lexeme.front() == '"' && lexeme.back() == '"') {
    std::string inner = lexeme.substr(1, lexeme.size() - 2);
    std::string result;
    result.reserve(inner.size());
    for (size_t i = 0; i < inner.size(); ++i) {
      if (inner[i] == '\\' && i + 1 < inner.size()) {
        char next = inner[i + 1];
        switch (next) {
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case '\\': result += '\\'; break;
        case '"': result += '"'; break;
        default: 
          // Preserve backslash if unknown escape
          result += '\\'; 
          result += next; 
          break;
        }
        i++; // skip next
      } else {
        result += inner[i];
      }
    }
    return result;
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

  if (match({TokenType::This})) {
    return std::make_unique<ThisExpr>(previous());
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

  // Brace for Map Literals: { "key": val, ... }
  if (match({TokenType::LeftBrace})) {
    std::vector<ExprPtr> keys;
    std::vector<ExprPtr> values;
    if (!check(TokenType::RightBrace)) {
      do {
        keys.push_back(expression());
        // I will use `Colon` (:) since I added it.
        consume(TokenType::Colon, "Expected ':' in map entry.");
        values.push_back(expression());
      } while (match({TokenType::Comma}));
    }
    consume(TokenType::RightBrace, "Expected '}' after map entries.");
    return std::make_unique<MapExpr>(std::move(keys), std::move(values));
  }

  if (match({TokenType::LeftParen})) {
    auto expr = expression();
    consume(TokenType::RightParen, "Expected ')' after expression.");
    return std::make_unique<GroupingExpr>(std::move(expr));
  }

  throw error(peek(), "Expected expression.");
}

StmtPtr Parser::functionDeclaration(Visibility vis) {
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
                                       std::move(body), vis);
}

StmtPtr Parser::classDeclaration(Visibility vis) {
  Token name = consume(TokenType::Identifier, "Expected class name.");
  consume(TokenType::LeftBrace, "Expected '{' before class body.");

  std::vector<std::shared_ptr<FuncDefStmt>> methods;
  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    consume(TokenType::Def, "Expected 'def' to define method.");
    auto stmt = functionDeclaration();
    auto funcStmt = std::unique_ptr<FuncDefStmt>(
        static_cast<FuncDefStmt *>(stmt.release()));
    methods.push_back(std::move(funcStmt));
  }

  consume(TokenType::RightBrace, "Expected '}' after class body.");

  return std::make_unique<ClassStmt>(name, methods, vis);
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
  auto expr = expression();

  if (match({TokenType::Swap})) {
    if (auto v = dynamic_cast<VariableExpr *>(expr.get())) {
      Token right =
          consume(TokenType::Identifier, "Expected identifier after '<->'.");
      match({TokenType::Semicolon});
      return std::make_unique<SwapStmt>(v->name, right);
    }
    throw error(previous(), "Invalid swap target.");
  }

  if (match({TokenType::Equal})) {
    auto value = expression();
    match({TokenType::Semicolon});

    if (auto v = dynamic_cast<VariableExpr *>(expr.get())) {
      return std::make_unique<VarAssignStmt>(v->name, std::move(value));
    }
    if (auto get = dynamic_cast<GetExpr *>(expr.get())) {
      return std::make_unique<ExprStmt>(std::make_unique<SetExpr>(
          std::move(get->object), get->name, std::move(value)));
    }
    if (auto idx = dynamic_cast<IndexExpr *>(expr.get())) {
      return std::make_unique<ExprStmt>(std::make_unique<IndexSetExpr>(
          std::move(idx->object), idx->bracket, std::move(idx->index),
          std::move(value)));
    }
    throw error(previous(), "Invalid assignment target.");
  }

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

ExprPtr Parser::expression() { return logicalOr(); }

ExprPtr Parser::logicalOr() {
  auto expr = logicalAnd();
  while (match({TokenType::Or})) {
    Token op = previous();
    auto right = logicalAnd();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::logicalAnd() {
  auto expr = equality();
  while (match({TokenType::And})) {
    Token op = previous();
    auto right = equality();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

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
  if (match({TokenType::Bang, TokenType::Minus, TokenType::Not})) {
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
    case TokenType::Class:
    case TokenType::If:
    case TokenType::While:
    case TokenType::Return:
    case TokenType::Print:
    case TokenType::Else:
    case TokenType::Module:
    case TokenType::Use:
    case TokenType::Open:
    case TokenType::Closed:
      return;
    default:
      break;
    }
    advance();
  }
}
