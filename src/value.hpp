#pragma once
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "token.hpp"

class Environment;

struct Function {
  Token name;
  std::vector<Token> params;
  const BlockStmt *body = nullptr;
  std::shared_ptr<Environment> closure;

  size_t arity() const { return params.size(); }
};

using FunctionPtr = std::shared_ptr<Function>;
struct List; // Forward decl
using ListPtr = std::shared_ptr<List>;

using Value = std::variant<std::monostate, double, std::string, bool,
                           FunctionPtr, ListPtr>; // monostate = nil

struct List {
  std::vector<Value> elements;
};

inline bool isNil(const Value &v) {
  return std::holds_alternative<std::monostate>(v);
}

inline bool isTruthy(const Value &v) {
  if (std::holds_alternative<std::monostate>(v))
    return false;
  if (auto b = std::get_if<bool>(&v))
    return *b;
  return true; // numbers, strings, functions, lists are truthy
}

inline std::string numberToString(double d) {
  if (std::isfinite(d)) {
    double ip;
    if (std::modf(d, &ip) == 0.0) {
      long long asInt = static_cast<long long>(ip);
      return std::to_string(asInt);
    }
  }
  std::ostringstream oss;
  oss << std::setprecision(15) << d;
  std::string s = oss.str();
  // trim trailing zeros
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0')
      s.pop_back();
    if (!s.empty() && s.back() == '.')
      s.pop_back();
  }
  return s;
}

inline std::string valueToString(const Value &v) {
  if (std::holds_alternative<std::monostate>(v))
    return "nil";
  if (auto d = std::get_if<double>(&v))
    return numberToString(*d);
  if (auto s = std::get_if<std::string>(&v))
    return *s;
  if (auto b = std::get_if<bool>(&v))
    return *b ? "true" : "false";
  if (auto f = std::get_if<FunctionPtr>(&v))
    return "<fn " + (*f)->name.lexeme + ">";
  if (auto l = std::get_if<ListPtr>(&v)) {
    std::string s = "[";
    const auto &elems = (*l)->elements;
    for (size_t i = 0; i < elems.size(); i++) {
      if (i > 0)
        s += ", ";
      s += valueToString(elems[i]);
    }
    s += "]";
    return s;
  }
  return "<value>";
}

inline bool valuesEqual(const Value &a, const Value &b) {
  if (a.index() != b.index()) {
    // allow nil == nil only, otherwise different types are not equal
    return isNil(a) && isNil(b);
  }
  if (isNil(a) && isNil(b))
    return true;
  if (auto da = std::get_if<double>(&a))
    return *da == std::get<double>(b);
  if (auto sa = std::get_if<std::string>(&a))
    return *sa == std::get<std::string>(b);
  if (auto ba = std::get_if<bool>(&a))
    return *ba == std::get<bool>(b);
  if (auto fa = std::get_if<FunctionPtr>(&a))
    return fa->get() == std::get<FunctionPtr>(b).get();
  if (auto la = std::get_if<ListPtr>(&a))
    return la->get() ==
           std::get<ListPtr>(b).get(); // pointer equality for lists
  return false;
}
