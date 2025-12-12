#pragma once
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "token.hpp"
#include "value.hpp"

class Environment : public std::enable_shared_from_this<Environment> {
public:
  explicit Environment(std::shared_ptr<Environment> enclosing = nullptr)
      : enclosing_(std::move(enclosing)) {}

  void define(const std::string &name, Value value) {
    values_[name] = std::move(value);
  }

  bool has(const std::string &name) const {
    if (values_.find(name) != values_.end())
      return true;
    if (enclosing_)
      return enclosing_->has(name);
    return false;
  }

  Value get(const Token &name) const {
    auto it = values_.find(name.lexeme);
    if (it != values_.end())
      return it->second;
    if (enclosing_)
      return enclosing_->get(name);
    throw std::runtime_error("Undefined variable '" + name.lexeme +
                             "' at line " + std::to_string(name.line));
  }

  void assign(const Token &name, Value value) {
    auto it = values_.find(name.lexeme);
    if (it != values_.end()) {
      it->second = std::move(value);
      return;
    }
    if (enclosing_) {
      enclosing_->assign(name, std::move(value));
      return;
    }
    throw std::runtime_error("Undefined variable '" + name.lexeme +
                             "' at line " + std::to_string(name.line));
  }

  std::shared_ptr<Environment> enclosing() const { return enclosing_; }

private:
  std::unordered_map<std::string, Value> values_;
  std::shared_ptr<Environment> enclosing_;
};
