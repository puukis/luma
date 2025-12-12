#pragma once
#include <string>

enum class TokenType {
  // Single-character tokens
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  Comma,
  Dot,
  Semicolon,
  Colon,        // :
  LeftBracket,  // [
  RightBracket, // ]
  At,           // @

  Plus,
  Minus,
  Star,
  Slash,

  // One or two character tokens
  Bang,
  BangEqual,
  Equal,
  EqualEqual,
  Greater,
  GreaterEqual,
  Less,
  LessEqual,
  Swap, // <->

  // Literals
  Identifier,
  Number,
  String,

  // Keywords
  Def,
  Return,
  If,
  Else,
  While,
  Class, // Class definition
  This,  // 'this' keyword
  True,
  False,
  Nil,
  Print,
  Echo,      // Luma unique: echo N { }
  Maybe,     // Luma unique: maybe { } otherwise { }
  Otherwise, // Luma unique: otherwise block
  Until,     // until (cond)

  // Module system
  Module,    // module declaration
  Use,       // use import
  As,        // as alias
  Open,      // open visibility (public)
  Closed,    // closed visibility (private)

  Eof
};

inline const char *tokenTypeName(TokenType t) {
  switch (t) {
  case TokenType::LeftParen:
    return "LeftParen";
  case TokenType::RightParen:
    return "RightParen";
  case TokenType::LeftBrace:
    return "LeftBrace";
  case TokenType::RightBrace:
    return "RightBrace";
  case TokenType::Comma:
    return "Comma";
  case TokenType::Dot:
    return "Dot";
  case TokenType::Semicolon:
    return "Semicolon";
  case TokenType::Colon:
    return "Colon";
  case TokenType::LeftBracket:
    return "LeftBracket";
  case TokenType::RightBracket:
    return "RightBracket";
  case TokenType::At:
    return "At";
  case TokenType::Plus:
    return "Plus";
  case TokenType::Minus:
    return "Minus";
  case TokenType::Star:
    return "Star";
  case TokenType::Slash:
    return "Slash";
  case TokenType::Bang:
    return "Bang";
  case TokenType::BangEqual:
    return "BangEqual";
  case TokenType::Equal:
    return "Equal";
  case TokenType::EqualEqual:
    return "EqualEqual";
  case TokenType::Greater:
    return "Greater";
  case TokenType::GreaterEqual:
    return "GreaterEqual";
  case TokenType::Less:
    return "Less";
  case TokenType::LessEqual:
    return "LessEqual";
  case TokenType::Swap:
    return "Swap";
  case TokenType::Identifier:
    return "Identifier";
  case TokenType::Number:
    return "Number";
  case TokenType::String:
    return "String";
  case TokenType::Def:
    return "Def";
  case TokenType::Return:
    return "Return";
  case TokenType::If:
    return "If";
  case TokenType::Else:
    return "Else";
  case TokenType::While:
    return "While";
  case TokenType::Class:
    return "Class";
  case TokenType::This:
    return "This";
  case TokenType::True:
    return "True";
  case TokenType::False:
    return "False";
  case TokenType::Nil:
    return "Nil";
  case TokenType::Print:
    return "Print";
  case TokenType::Echo:
    return "Echo";
  case TokenType::Maybe:
    return "Maybe";
  case TokenType::Otherwise:
    return "Otherwise";
  case TokenType::Until:
    return "Until";
  case TokenType::Module:
    return "Module";
  case TokenType::Use:
    return "Use";
  case TokenType::As:
    return "As";
  case TokenType::Open:
    return "Open";
  case TokenType::Closed:
    return "Closed";
  case TokenType::Eof:
    return "Eof";
  }
  return "Unknown";
}

struct Token {
  TokenType type;
  std::string lexeme; // the exact text
  int line;
};
