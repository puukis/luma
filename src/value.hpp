#pragma once
#include <cmath>
#include <functional>
#include <iomanip>
#include <map>
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
struct LumaClass;
using ClassPtr = std::shared_ptr<LumaClass>;
struct LumaInstance;
using InstancePtr = std::shared_ptr<LumaInstance>;
struct LumaMap;
using MapPtr = std::shared_ptr<LumaMap>;

// Forward declaration of Value for NativeFunction
struct ValueWrapper;
// We need a recursive definition for NativeFunction -> Value -> NativeFunction.
// But std::variant is fine with incomplete types if we use pointers? No, Value
// is by value. NativeFunction is std::function. std::function needs complete
// type for operator(). But we can define NativeFunction as taking
// vector<variant...>. Wait, Value is defined right here.

// Circular dependency: Value depends on NativeFunction, NativeFunction depends
// on Value. Solution: Use a struct wrapper or declared type.
// Actually, std::function<Value(const std::vector<Value>&)> works if Value is
// forward declared? No, it needs to know size.
// The standard way is using std::any or void*, or a wrapper struct.
// Let's forward declare Value, but we can't fully define NativeFunction before
// Value.
// HOWEVER, std::function types are erased. We can define NativeFunction with a
// forward declared Value? No.
// Let's define Value first, but without NativeFunction, then add it? No.
//
// Workaround: Define NativeFunction as a struct that holds the std::function?
// Or simply use std::shared_ptr<NativeFunctionImpl> where NativeFunctionImpl
// is defined later?
// Lox uses a class for NativeFunction. Let's use a shared pointer to a struct.

struct NativeFunctionObject;
using NativeFunctionPtr = std::shared_ptr<NativeFunctionObject>;

using Value =
    std::variant<std::monostate, double, std::string, bool, FunctionPtr,
                 ListPtr, ClassPtr, InstancePtr, MapPtr, NativeFunctionPtr>; // monostate = nil

struct NativeFunctionObject {
    std::function<Value(const std::vector<Value>&)> func;
    std::string name; // for debugging
    size_t arity = 0; // -1 for variadic? Let's generic size_t.
    bool variadic = false;
};

struct List {
  std::vector<Value> elements;
};

struct LumaMap {
  std::map<std::string, Value> values;
};

struct LumaClass {
  std::string name;
  std::map<std::string, FunctionPtr> methods;

  FunctionPtr findMethod(const std::string &name) {
    if (methods.count(name))
      return methods.at(name);
    return nullptr;
  }
};

struct LumaInstance {
  ClassPtr klass;
  std::map<std::string, Value> fields;

  LumaInstance(ClassPtr k) : klass(k) {}
};

inline bool isNil(const Value &v) {
  return std::holds_alternative<std::monostate>(v);
}

inline bool isTruthy(const Value &v) {
  if (std::holds_alternative<std::monostate>(v))
    return false;
  if (auto b = std::get_if<bool>(&v))
    return *b;
  return true; // numbers, strings, functions, lists, classes, instances, maps
               // are truthy
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
  if (auto n = std::get_if<NativeFunctionPtr>(&v))
      return "<native fn " + (*n)->name + ">";
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
  if (auto c = std::get_if<ClassPtr>(&v)) {
    return "<class " + (*c)->name + ">";
  }
  if (auto i = std::get_if<InstancePtr>(&v)) {
    return "<instance " + (*i)->klass->name + ">";
  }
  if (auto m = std::get_if<MapPtr>(&v)) {
    std::string s = "{";
    const auto &map = (*m)->values;
    int k = 0;
    for (const auto &[key, val] : map) {
      if (k > 0)
        s += ", ";
      s += key + ": " + valueToString(val);
      k++;
    }
    s += "}";
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
  if (auto ca = std::get_if<ClassPtr>(&a))
    return ca->get() == std::get<ClassPtr>(b).get();
  if (auto ia = std::get_if<InstancePtr>(&a))
    return ia->get() == std::get<InstancePtr>(b).get();
  if (auto ma = std::get_if<MapPtr>(&a))
    return ma->get() == std::get<MapPtr>(b).get();
   if (auto na = std::get_if<NativeFunctionPtr>(&a))
    return na->get() == std::get<NativeFunctionPtr>(b).get(); // pointer equality
  return false;
}
