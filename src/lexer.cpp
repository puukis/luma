#include "lexer.hpp"
#include <stdexcept>
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> kKeywords = {
    {"def", TokenType::Def},     {"return", TokenType::Return},
    {"if", TokenType::If},       {"else", TokenType::Else},
    {"while", TokenType::While}, {"true", TokenType::True},
    {"false", TokenType::False}, {"nil", TokenType::Nil},
    {"print", TokenType::Print}, {"echo", TokenType::Echo},
    {"maybe", TokenType::Maybe}, {"otherwise", TokenType::Otherwise},
    {"maybe", TokenType::Maybe}, {"otherwise", TokenType::Otherwise},
    {"until", TokenType::Until}, {"class", TokenType::Class},
    {"this", TokenType::This},
};

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::scanTokens() {
  while (!isAtEnd()) {
    start_ = current_;
    scanToken();
  }
  tokens_.push_back({TokenType::Eof, "", line_});
  return tokens_;
}

bool Lexer::isAtEnd() const { return current_ >= source_.size(); }

char Lexer::advance() { return source_[current_++]; }

char Lexer::peek() const {
  if (isAtEnd())
    return '\0';
  return source_[current_];
}

char Lexer::peekNext() const {
  if (current_ + 1 >= source_.size())
    return '\0';
  return source_[current_ + 1];
}

bool Lexer::match(char expected) {
  if (isAtEnd())
    return false;
  if (source_[current_] != expected)
    return false;
  current_++;
  return true;
}

void Lexer::addToken(TokenType type) {
  tokens_.push_back({type, source_.substr(start_, current_ - start_), line_});
}

bool Lexer::isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool Lexer::isDigit(char c) { return (c >= '0' && c <= '9'); }
bool Lexer::isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c); }

void Lexer::scanToken() {
  char c = advance();
  switch (c) {
  case '(':
    addToken(TokenType::LeftParen);
    break;
  case ')':
    addToken(TokenType::RightParen);
    break;
  case '{':
    addToken(TokenType::LeftBrace);
    break;
  case '}':
    addToken(TokenType::RightBrace);
    break;
  case ',':
    addToken(TokenType::Comma);
    break;
  case '.':
    addToken(TokenType::Dot);
    break;
  case ';':
    addToken(TokenType::Semicolon);
    break;
  case ':':
    addToken(TokenType::Colon);
    break;
  case '[':
    addToken(TokenType::LeftBracket);
    break;
  case ']':
    addToken(TokenType::RightBracket);
    break;
  case '+':
    addToken(TokenType::Plus);
    break;
  case '-':
    addToken(TokenType::Minus);
    break;
  case '*':
    addToken(TokenType::Star);
    break;

  case '!':
    addToken(match('=') ? TokenType::BangEqual : TokenType::Bang);
    break;
  case '=':
    addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
    break;
  case '<':
    if (match('-')) {
      if (match('>')) {
        addToken(TokenType::Swap); // <->
      } else {
        throw std::runtime_error("Unexpected '<-' at line " +
                                 std::to_string(line_) +
                                 ", did you mean '<->'?");
      }
    } else {
      addToken(match('=') ? TokenType::LessEqual : TokenType::Less);
    }
    break;
  case '>':
    addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater);
    break;

  case '/':
    if (match('/')) { // comment until end of line
      while (peek() != '\n' && !isAtEnd())
        advance();
    } else {
      addToken(TokenType::Slash);
    }
    break;

  case ' ':
  case '\r':
  case '\t':
    break;

  case '\n':
    line_++;
    break;

  case '"':
    stringLiteral();
    break;

  default:
    if (isDigit(c)) {
      numberLiteral();
    } else if (isAlpha(c)) {
      identifierOrKeyword();
    } else {
      throw std::runtime_error("Unexpected character at line " +
                               std::to_string(line_) + ": '" +
                               std::string(1, c) + "'");
    }
  }
}

void Lexer::stringLiteral() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n')
      line_++;
    advance();
  }

  if (isAtEnd()) {
    throw std::runtime_error("Unterminated string at line " +
                             std::to_string(line_));
  }

  advance(); // closing "
  addToken(TokenType::String);
}

void Lexer::numberLiteral() {
  while (isDigit(peek()))
    advance();

  if (peek() == '.' && isDigit(peekNext())) {
    advance(); // consume '.'
    while (isDigit(peek()))
      advance();
  }

  addToken(TokenType::Number);
}

void Lexer::identifierOrKeyword() {
  while (isAlphaNumeric(peek()))
    advance();

  std::string text = source_.substr(start_, current_ - start_);
  auto it = kKeywords.find(text);
  if (it != kKeywords.end()) {
    tokens_.push_back({it->second, text, line_});
  } else {
    tokens_.push_back({TokenType::Identifier, text, line_});
  }
}
