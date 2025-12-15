#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace fs = std::filesystem;

// Forward decls for global natives
static Value nativeLen(const std::vector<Value> &args);
static Value nativePush(const std::vector<Value> &args);
static Value nativePop(const std::vector<Value> &args);
static Value nativeKeys(const std::vector<Value> &args);
static Value nativeRemove(const std::vector<Value> &args);

// Path module natives
static Value nativePathJoin(const std::vector<Value> &args);
static Value nativePathDirname(const std::vector<Value> &args);
static Value nativePathBasename(const std::vector<Value> &args);
static Value nativePathExtname(const std::vector<Value> &args);
static Value nativePathStem(const std::vector<Value> &args);
static Value nativePathNormalize(const std::vector<Value> &args);
static Value nativePathAbsolute(const std::vector<Value> &args);
static Value nativePathIsAbsolute(const std::vector<Value> &args);
static Value nativePathRelative(const std::vector<Value> &args);
static Value nativePathResolve(const std::vector<Value> &args);
static Value nativePathSep(const std::vector<Value> &args);
static Value nativePathDelimiter(const std::vector<Value> &args);

// Encoding module natives
static Value nativeEncodingBase64Encode(const std::vector<Value> &args);
static Value nativeEncodingBase64Decode(const std::vector<Value> &args);
static Value nativeEncodingUrlEncode(const std::vector<Value> &args);
static Value nativeEncodingUrlDecode(const std::vector<Value> &args);
static Value nativeEncodingHexEncode(const std::vector<Value> &args);
static Value nativeEncodingHexDecode(const std::vector<Value> &args);
static Value nativeEncodingHtmlEscape(const std::vector<Value> &args);
static Value nativeEncodingHtmlUnescape(const std::vector<Value> &args);

// DateTime module natives
static Value nativeDateTimeNow(const std::vector<Value> &args);
static Value nativeDateTimeParse(const std::vector<Value> &args);
static Value nativeDateTimeFromComponents(const std::vector<Value> &args);
static Value nativeDateTimeYear(const std::vector<Value> &args);
static Value nativeDateTimeMonth(const std::vector<Value> &args);
static Value nativeDateTimeDay(const std::vector<Value> &args);
static Value nativeDateTimeFormat(const std::vector<Value> &args);
static Value nativeDateTimeDayOfWeek(const std::vector<Value> &args);
static Value nativeDateTimeDayOfYear(const std::vector<Value> &args);
static Value nativeDateTimeIsLeapYear(const std::vector<Value> &args);
static Value nativeDateTimeDaysInMonth(const std::vector<Value> &args);

// Sys module natives
static Value nativeSysPlatform(const std::vector<Value> &args);
static Value nativeSysArch(const std::vector<Value> &args);
static Value nativeSysPlatformInfo(const std::vector<Value> &args);
static Value nativeSysCpuCount(const std::vector<Value> &args);
static Value nativeSysCpuInfo(const std::vector<Value> &args);
static Value nativeSysTotalMemory(const std::vector<Value> &args);
static Value nativeSysAvailableMemory(const std::vector<Value> &args);
static Value nativeSysMemoryInfo(const std::vector<Value> &args);
static Value nativeSysPid(const std::vector<Value> &args);
static Value nativeSysPpid(const std::vector<Value> &args);
static Value nativeSysProcessInfo(const std::vector<Value> &args);
static Value nativeSysLoadAverage(const std::vector<Value> &args);
static Value nativeSysUptime(const std::vector<Value> &args);
static Value nativeSysHostname(const std::vector<Value> &args);
static Value nativeSysNetworkInterfaces(const std::vector<Value> &args);
static Value nativeSysExecutablePath(const std::vector<Value> &args);
static Value nativeSysCwd(const std::vector<Value> &args);
static Value nativeSysEnviron(const std::vector<Value> &args);
static Value nativeSysExit(const std::vector<Value> &args);
static Value nativeSysArgv(const std::vector<Value> &args);
static Value nativeSysProgname(const std::vector<Value> &args);

// UUID module natives
static Value nativeUuidV4(const std::vector<Value> &args);
static Value nativeUuidNil(const std::vector<Value> &args);
static Value nativeUuidIsValid(const std::vector<Value> &args);
static Value nativeUuidParse(const std::vector<Value> &args);
static Value nativeUuidStringify(const std::vector<Value> &args);

// URL module natives
static Value nativeUrlParse(const std::vector<Value> &args);
static Value nativeUrlFormat(const std::vector<Value> &args);
static Value nativeUrlParseQuery(const std::vector<Value> &args);
static Value nativeUrlBuildQuery(const std::vector<Value> &args);
static Value nativeUrlResolve(const std::vector<Value> &args);

// Async module natives
static Value nativeAsyncSleep(const std::vector<Value> &args);

// Net module natives
static Value nativeNetIsIpv4(const std::vector<Value> &args);
static Value nativeNetIsIpv6(const std::vector<Value> &args);
static Value nativeNetIpv4ToInt(const std::vector<Value> &args);
static Value nativeNetIntToIpv4(const std::vector<Value> &args);
static Value nativeNetDnsLookup(const std::vector<Value> &args);
static Value nativeNetGetHostname(const std::vector<Value> &args);
static Value nativeNetParseUrl(const std::vector<Value> &args);

// Socket module natives
static Value nativeSocketCreate(const std::vector<Value> &args);
static Value nativeSocketBind(const std::vector<Value> &args);
static Value nativeSocketListen(const std::vector<Value> &args);
static Value nativeSocketAccept(const std::vector<Value> &args);
static Value nativeSocketConnect(const std::vector<Value> &args);
static Value nativeSocketSend(const std::vector<Value> &args);
static Value nativeSocketRecv(const std::vector<Value> &args);
static Value nativeSocketSendTo(const std::vector<Value> &args);
static Value nativeSocketRecvFrom(const std::vector<Value> &args);
static Value nativeSocketClose(const std::vector<Value> &args);
static Value nativeSocketSetOption(const std::vector<Value> &args);
static Value nativeSocketGetOption(const std::vector<Value> &args);


Interpreter::Interpreter() {
  globals_ = std::make_shared<Environment>();
  env_ = globals_;

  auto defineGlobal = [&](const std::string &name, std::function<Value(const std::vector<Value> &)> func, size_t arity) {
    auto native = std::make_shared<NativeFunctionObject>();
    native->name = name;
    native->func = func;
    native->arity = arity;
    globals_->define(name, native);
  };

  defineGlobal("len", nativeLen, 1);
  defineGlobal("push", nativePush, 2);
  defineGlobal("pop", nativePop, 1);
  defineGlobal("keys", nativeKeys, 1);
  defineGlobal("remove", nativeRemove, 2);
}

void Interpreter::setExecutablePath(const std::string &path) {
  executablePath_ = fs::absolute(path).string();
}

void Interpreter::setEntryFile(const std::string &path) {
  entryFilePath_ = fs::absolute(path).string();
  initializeRoots();
}

void Interpreter::initializeRoots() {
  if (entryFilePath_.empty())
    return;

  // Project root: directory containing the entry file, or its parent if in src/
  fs::path entryPath(entryFilePath_);
  fs::path entryDir = entryPath.parent_path();

  // Check if we're in a src/ subdirectory
  if (entryDir.filename() == "src") {
    projectRoot_ = entryDir.parent_path().string();
  } else {
    projectRoot_ = entryDir.string();
  }

  // Stdlib root:
  // 1. Check installed "Module" folder next to executable
  if (!executablePath_.empty()) {
    fs::path exePath(executablePath_);
    fs::path installedModules = exePath.parent_path() / "Module";
    if (fs::exists(installedModules) && fs::is_directory(installedModules)) {
      stdlibRoot_ = installedModules.string();
    }
  }

  // 2. Check local "std" folder in project root (overrides installed for dev)
  fs::path projectStd = fs::path(projectRoot_) / "std";
  if (fs::exists(projectStd) && fs::is_directory(projectStd)) {
    stdlibRoot_ = projectStd.string();
  }

  // 3. Last fallback: current working directory / std
  if (stdlibRoot_.empty()) {
    fs::path cwdStd = fs::current_path() / "std";
    if (fs::exists(cwdStd) && fs::is_directory(cwdStd)) {
      stdlibRoot_ = cwdStd.string();
    }
  }
}

void Interpreter::run(const std::vector<StmtPtr> &program) {
  try {
    for (const auto &s : program) {
      execute(*s);
    }
  } catch (const ReturnSignal &) {
    throw std::runtime_error("Return used outside of a function.");
  }
}

void Interpreter::assignOrDefine(const Token &name, Value value) {
  if (env_->has(name.lexeme)) {
    env_->assign(name, std::move(value));
  } else {
    env_->define(name.lexeme, std::move(value));
  }
}

void Interpreter::executeBlock(const BlockStmt &block,
                               std::shared_ptr<Environment> newEnv) {
  std::shared_ptr<Environment> previous = env_;
  try {
    env_ = newEnv;
    for (const auto &stmt : block.statements) {
      execute(*stmt);
    }
  } catch (...) {
    env_ = previous;
    throw;
  }
  env_ = previous;
}

void Interpreter::visitClassStmt(const ClassStmt &stmt) {
  env_->define(stmt.name.lexeme, std::monostate{});

  std::map<std::string, FunctionPtr> methods;
  for (const auto &method : stmt.methods) {
    auto func = std::make_shared<Function>();
    func->name = method->name;
    func->params = method->params;
    func->body = method->body.get();
    func->closure = env_; // Closure captures defining scope
    methods[method->name.lexeme] = func;
  }

  auto klass = std::make_shared<LumaClass>();
  klass->name = stmt.name.lexeme;
  klass->methods = std::move(methods);

  env_->assign(stmt.name, klass);
}

void Interpreter::execute(const Stmt &stmt) {
  if (auto *x = dynamic_cast<const ExprStmt *>(&stmt)) {
    (void)evaluate(*x->expr);
    return;
  }

  if (auto *p = dynamic_cast<const PrintStmt *>(&stmt)) {
    Value v = evaluate(*p->expr);
    std::cout << valueToString(v) << "\n";
    return;
  }

  if (auto *a = dynamic_cast<const VarAssignStmt *>(&stmt)) {
    Value v = evaluate(*a->value);
    assignOrDefine(a->name, std::move(v));
    return;
  }

  if (auto *b = dynamic_cast<const BlockStmt *>(&stmt)) {
    auto newEnv = std::make_shared<Environment>(env_);
    executeBlock(*b, std::move(newEnv));
    return;
  }

  if (auto *i = dynamic_cast<const IfStmt *>(&stmt)) {
    Value cond = evaluate(*i->condition);
    if (isTruthy(cond)) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*i->thenBranch, std::move(newEnv));
    } else if (i->elseBranch) {
      // elseBranch can be a BlockStmt or another IfStmt (else if)
      if (auto *elseBlock =
              dynamic_cast<const BlockStmt *>(i->elseBranch.get())) {
        auto newEnv = std::make_shared<Environment>(env_);
        executeBlock(*elseBlock, std::move(newEnv));
      } else {
        // It's another IfStmt (else if chain)
        execute(*i->elseBranch);
      }
    }
    return;
  }

  if (auto *w = dynamic_cast<const WhileStmt *>(&stmt)) {
    while (isTruthy(evaluate(*w->condition))) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*w->body, std::move(newEnv));
    }
    return;
  }

  if (auto *u = dynamic_cast<const UntilStmt *>(&stmt)) {
    // Until = while (!condition)
    while (!isTruthy(evaluate(*u->condition))) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*u->body, std::move(newEnv));
    }
    return;
  }

  if (auto *r = dynamic_cast<const ReturnStmt *>(&stmt)) {
    Value v = std::monostate{};
    if (r->value)
      v = evaluate(*r->value);
    throw ReturnSignal{std::move(v)};
  }

  if (auto *f = dynamic_cast<const FuncDefStmt *>(&stmt)) {
    auto fn = std::make_shared<Function>();
    fn->name = f->name;
    fn->params = f->params;
    fn->body = f->body.get();
    fn->closure = env_; // capture defining environment
    env_->define(f->name.lexeme, fn);
    return;
  }

  if (auto *c = dynamic_cast<const ClassStmt *>(&stmt)) {
    visitClassStmt(*c);
    return;
  }

  // ========== Luma Unique Statements ==========

  // echo N { ... } - repeat block N times
  if (auto *e = dynamic_cast<const EchoStmt *>(&stmt)) {
    Value countVal = evaluate(*e->count);
    if (!std::holds_alternative<double>(countVal)) {
      throw std::runtime_error("Echo count must be a number.");
    }
    int count = static_cast<int>(std::get<double>(countVal));
    if (count < 0) {
      throw std::runtime_error("Echo count cannot be negative.");
    }
    for (int i = 0; i < count; i++) {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*e->body, std::move(newEnv));
    }
    return;
  }

  // a <-> b - swap two variables
  if (auto *s = dynamic_cast<const SwapStmt *>(&stmt)) {
    Value leftVal = env_->get(s->left);
    Value rightVal = env_->get(s->right);
    env_->assign(s->left, std::move(rightVal));
    env_->assign(s->right, std::move(leftVal));
    return;
  }

  // maybe { ... } otherwise { ... } - error handling
  if (auto *m = dynamic_cast<const MaybeStmt *>(&stmt)) {
    try {
      auto newEnv = std::make_shared<Environment>(env_);
      executeBlock(*m->tryBlock, std::move(newEnv));
    } catch (const std::runtime_error &) {
      // If an error occurred and we have an otherwise block, execute it
      if (m->otherwiseBlock) {
        auto newEnv = std::make_shared<Environment>(env_);
        executeBlock(*m->otherwiseBlock, std::move(newEnv));
      }
      // If no otherwise block, silently continue (that's the "maybe"
      // philosophy)
    }
    return;
  }

  // ========== Module System Statements ==========

  // module @std.io - record current module identity
  if (auto *mod = dynamic_cast<const ModuleStmt *>(&stmt)) {
    currentModuleId_ = moduleIdToString(mod->moduleIdParts);
    // Validate: user code should not declare @std.* modules
    if (currentModuleId_.rfind("@std.", 0) == 0 && !stdlibRoot_.empty()) {
      // Check if we're actually in the stdlib directory
      fs::path currentFile(entryFilePath_);
      fs::path stdPath(stdlibRoot_);
      // For now, allow it (stdlib files need to declare their identity)
    }
    return;
  }

  // use @std.io as io - load module and bind to alias
  if (auto *use = dynamic_cast<const UseStmt *>(&stmt)) {
    std::string moduleId = moduleIdToString(use->moduleIdParts);
    MapPtr exports = loadModule(moduleId);
    env_->define(use->alias.lexeme, exports);
    return;
  }

  throw std::runtime_error("Unknown statement at runtime.");
}

static void requireNumber(const Value &v, const std::string &where) {
  if (!std::holds_alternative<double>(v)) {
    throw std::runtime_error("Type error: expected number in " + where +
                             ", got " + valueToString(v));
  }
}

Value Interpreter::callFunction(const Value &callee,
                                const std::vector<Value> &args,
                                const Token &callSiteParen) {
  if (auto fn = std::get_if<FunctionPtr>(&callee)) {
    FunctionPtr function = *fn;
    if (args.size() != function->arity()) {
      throw std::runtime_error("Expected " + std::to_string(function->arity()) +
                               " arguments but got " +
                               std::to_string(args.size()) + ".");
    }

    auto environment = std::make_shared<Environment>(function->closure);
    for (size_t i = 0; i < function->params.size(); ++i) {
      environment->define(function->params[i].lexeme, args[i]);
    }

    try {
      executeBlock(*function->body, environment);
    } catch (const ReturnSignal &returnValue) {
      return returnValue.value;
    }
    return std::monostate{};
  }

  if (auto nf = std::get_if<NativeFunctionPtr>(&callee)) {
      NativeFunctionPtr native = *nf;
      if (!native->variadic && args.size() != native->arity) {
          throw std::runtime_error("Expected " + std::to_string(native->arity) +
              " arguments but got " +
              std::to_string(args.size()) + ".");
      }
      return native->func(args);
  }

  if (auto klass = std::get_if<ClassPtr>(&callee)) {
    // Create instance
    auto instance = std::make_shared<LumaInstance>(*klass);

    // Look for definition of "init"
    FunctionPtr init = (*klass)->findMethod("init");
    if (init) {
      // Bind 'this' to init
      // We need to bind 'this' to the method call.
      // Current implementation of Function doesn't support 'this' binding
      // implicitly? We need a way to bind 'this'. Standard Lox way:
      // Function.bind(instance) -> new Function with environment containing
      // "this".

      auto environment = std::make_shared<Environment>(init->closure);
      environment->define("this", instance); // Bind 'this'

      // Handle args
      if (args.size() != init->arity()) {
        throw std::runtime_error("Expected " + std::to_string(init->arity()) +
                                 " arguments but got " +
                                 std::to_string(args.size()) + ".");
      }
      for (size_t i = 0; i < init->params.size(); ++i) {
        environment->define(init->params[i].lexeme, args[i]);
      }

      try {
        executeBlock(*init->body, environment);
      } catch (const ReturnSignal &returnValue) {
        // initializer usually returns 'this', but if user returns something
        // else? Lox: always return this. Luma: let's return this implicitly
        // unless explicit return? For now, ignore return value of init and
        // return instance? Or allow explicit return? Lox ignores explicit
        // return in init. Let's mimic Lox: init returns 'this'.
      }
    } else {
      if (!args.empty()) {
        throw std::runtime_error("Expected 0 arguments but got " +
                                 std::to_string(args.size()) + ".");
      }
    }

    return instance;
  }

  throw std::runtime_error("Can only call functions and classes.");
}
Value Interpreter::evaluate(const Expr &expr) {
  if (auto *l = dynamic_cast<const LiteralExpr *>(&expr)) {
    switch (l->kind) {
    case LiteralExpr::Kind::Number:
      return l->numberValue;
    case LiteralExpr::Kind::String:
      return l->stringValue;
    case LiteralExpr::Kind::Bool:
      return l->boolValue;
    case LiteralExpr::Kind::Nil:
      return std::monostate{};
    }
  }

  if (auto *v = dynamic_cast<const VariableExpr *>(&expr)) {
    return env_->get(v->name);
  }

  if (auto *g = dynamic_cast<const GroupingExpr *>(&expr)) {
    return evaluate(*g->expr);
  }

  if (auto *u = dynamic_cast<const UnaryExpr *>(&expr)) {
    Value right = evaluate(*u->right);
    if (u->op.type == TokenType::Minus) {
      requireNumber(right, "unary '-'");
      return -std::get<double>(right);
    }
    if (u->op.type == TokenType::Bang || u->op.type == TokenType::Not) {
      return !isTruthy(right);
    }
    throw std::runtime_error("Unknown unary operator '" + u->op.lexeme + "'");
  }

  if (auto *b = dynamic_cast<const BinaryExpr *>(&expr)) {
    // Short-circuit evaluation for logical operators
    if (b->op.type == TokenType::Or) {
      Value left = evaluate(*b->left);
      if (isTruthy(left)) return left;  // Short-circuit: don't evaluate right
      return evaluate(*b->right);
    }
    if (b->op.type == TokenType::And) {
      Value left = evaluate(*b->left);
      if (!isTruthy(left)) return left;  // Short-circuit: don't evaluate right
      return evaluate(*b->right);
    }

    // Eager evaluation for other binary operators
    Value left = evaluate(*b->left);
    Value right = evaluate(*b->right);

    switch (b->op.type) {
    case TokenType::Plus:
      if (std::holds_alternative<double>(left) &&
          std::holds_alternative<double>(right)) {
        return std::get<double>(left) + std::get<double>(right);
      }
      if (std::holds_alternative<std::string>(left) &&
          std::holds_alternative<std::string>(right)) {
        return std::get<std::string>(left) + std::get<std::string>(right);
      }
      throw std::runtime_error(
          "Type error: '+' needs (number,number) or (string,string).");
    case TokenType::Minus:
      requireNumber(left, "binary '-'");
      requireNumber(right, "binary '-'");
      return std::get<double>(left) - std::get<double>(right);
    case TokenType::Star:
      requireNumber(left, "binary '*'");
      requireNumber(right, "binary '*'");
      return std::get<double>(left) * std::get<double>(right);
    case TokenType::Slash:
      requireNumber(left, "binary '/'");
      requireNumber(right, "binary '/'");
      if (std::get<double>(right) == 0.0)
        throw std::runtime_error("Runtime error: division by zero.");
      return std::get<double>(left) / std::get<double>(right);
    case TokenType::Ampersand:
      requireNumber(left, "bitwise '&'");
      requireNumber(right, "bitwise '&'");
      return static_cast<double>(static_cast<int64_t>(std::get<double>(left)) & static_cast<int64_t>(std::get<double>(right)));
    case TokenType::Pipe:
      requireNumber(left, "bitwise '|'");
      requireNumber(right, "bitwise '|'");
      return static_cast<double>(static_cast<int64_t>(std::get<double>(left)) | static_cast<int64_t>(std::get<double>(right)));
    case TokenType::ShiftLeft:
      requireNumber(left, "bitwise '<<'");
      requireNumber(right, "bitwise '<<'");
      return static_cast<double>(static_cast<int64_t>(std::get<double>(left)) << static_cast<int64_t>(std::get<double>(right)));
    case TokenType::ShiftRight:
      requireNumber(left, "bitwise '>>'");
      requireNumber(right, "bitwise '>>'");
      return static_cast<double>(static_cast<int64_t>(std::get<double>(left)) >> static_cast<int64_t>(std::get<double>(right)));
    case TokenType::Greater:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) > std::get<double>(right);
    case TokenType::GreaterEqual:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) >= std::get<double>(right);
    case TokenType::Less:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) < std::get<double>(right);
    case TokenType::LessEqual:
      requireNumber(left, "comparison");
      requireNumber(right, "comparison");
      return std::get<double>(left) <= std::get<double>(right);
    case TokenType::EqualEqual:
      return valuesEqual(left, right);
    case TokenType::BangEqual:
      return !valuesEqual(left, right);
    default:
      throw std::runtime_error("Unknown binary operator '" + b->op.lexeme +
                               "'");
    }
  }

  if (auto *c = dynamic_cast<const CallExpr *>(&expr)) {
    Value callee = evaluate(*c->callee);
    std::vector<Value> args;
    args.reserve(c->args.size());
    for (const auto &a : c->args)
      args.push_back(evaluate(*a));

    if (std::holds_alternative<FunctionPtr>(callee) ||
        std::holds_alternative<ClassPtr>(callee) ||
        std::holds_alternative<NativeFunctionPtr>(callee)) {
      return callFunction(callee, args, c->paren);
    }
    throw std::runtime_error("Can only call functions and classes.");
  }

  if (auto *indexExpr = dynamic_cast<const IndexExpr *>(&expr)) {
    Value object = evaluate(*indexExpr->object);
    Value index = evaluate(*indexExpr->index);

    if (auto listPtr = std::get_if<ListPtr>(&object)) {
      if (!std::holds_alternative<double>(index)) {
        throw std::runtime_error("List index must be a number.");
      }
      int idx = static_cast<int>(std::get<double>(index));
      auto &elements = (*listPtr)->elements;
      if (idx < 0 || idx >= static_cast<int>(elements.size())) {
        throw std::runtime_error("List index out of bounds.");
      }
      return elements[idx];
    }

    if (auto mapPtr = std::get_if<MapPtr>(&object)) {
      if (auto s = std::get_if<std::string>(&index)) {
        auto &values = (*mapPtr)->values;
        if (values.count(*s)) {
          return values.at(*s);
        }
        throw std::runtime_error("Undefined key '" + *s + "'.");
      }
      throw std::runtime_error("Map key must be a string.");
    }
    throw std::runtime_error("Only lists and maps support subscription.");
  }

  if (auto *indexSetExpr = dynamic_cast<const IndexSetExpr *>(&expr)) {
    Value object = evaluate(*indexSetExpr->object);
    Value index = evaluate(*indexSetExpr->index);
    Value value = evaluate(*indexSetExpr->value);

    if (auto listPtr = std::get_if<ListPtr>(&object)) {
      if (!std::holds_alternative<double>(index)) {
        throw std::runtime_error("List index must be a number.");
      }
      int idx = static_cast<int>(std::get<double>(index));
      auto &elements = (*listPtr)->elements;
      if (idx < 0 || idx >= static_cast<int>(elements.size())) {
        throw std::runtime_error("List index out of bounds.");
      }
      elements[idx] = value;
      return value;
    }

    if (auto mapPtr = std::get_if<MapPtr>(&object)) {
      if (auto s = std::get_if<std::string>(&index)) {
        (*mapPtr)->values[*s] = value;
        return value;
      }
      throw std::runtime_error("Map key must be a string.");
    }
    throw std::runtime_error("Only lists and maps support assignment.");
  }

  if (auto *listExpr = dynamic_cast<const ListExpr *>(&expr)) {
    auto list = std::make_shared<List>();
    for (const auto &e : listExpr->elements) {
      list->elements.push_back(evaluate(*e));
    }
    return list;
  }

  if (auto get = dynamic_cast<const GetExpr *>(&expr)) {
    Value object = evaluate(*get->object);
    // Module namespace access (MapPtr)
    if (auto mapPtr = std::get_if<MapPtr>(&object)) {
      const auto &values = (*mapPtr)->values;
      auto it = values.find(get->name.lexeme);
      if (it != values.end()) {
        return it->second;
      }
      throw std::runtime_error("Module has no exported member '" +
                               get->name.lexeme + "'.");
    }
    // Instance property access
    if (auto inst = std::get_if<InstancePtr>(&object)) {
      LumaInstance *instance = inst->get();
      if (instance->fields.count(get->name.lexeme)) {
        return instance->fields.at(get->name.lexeme);
      }
      FunctionPtr method = instance->klass->findMethod(get->name.lexeme);
      if (method) {
        auto newEnv = std::make_shared<Environment>(method->closure);
        newEnv->define("this", *inst);
        auto boundMethod = std::make_shared<Function>(*method);
        boundMethod->closure = newEnv;
        return boundMethod;
      }
      throw std::runtime_error("Undefined property '" + get->name.lexeme +
                               "'.");
    }
    throw std::runtime_error("Only instances and modules have properties.");
  }

  if (auto set = dynamic_cast<const SetExpr *>(&expr)) {
    Value object = evaluate(*set->object);
    if (auto inst = std::get_if<InstancePtr>(&object)) {
      Value value = evaluate(*set->value);
      (*inst)->fields[set->name.lexeme] = value;
      return value;
    }
    throw std::runtime_error("Only instances have properties.");
  }

  if (auto th = dynamic_cast<const ThisExpr *>(&expr)) {
    return env_->get(th->keyword);
  }

  if (auto mp = dynamic_cast<const MapExpr *>(&expr)) {
    auto map = std::make_shared<LumaMap>();
    for (size_t i = 0; i < mp->keys.size(); ++i) {
      Value k = evaluate(*mp->keys[i]);
      Value v = evaluate(*mp->values[i]);
      if (auto s = std::get_if<std::string>(&k)) {
        map->values[*s] = v;
      } else {
        throw std::runtime_error("Map keys must be strings.");
      }
    }
    return map;
  }

  throw std::runtime_error("Unknown expression type.");
}

// ========== Module System Implementation ==========

std::string Interpreter::moduleIdToString(const std::vector<Token> &parts) {
  // Convert [@, std, ., io] -> "@std.io"
  std::string result;
  for (const auto &tok : parts) {
    result += tok.lexeme;
  }
  return result;
}

std::string Interpreter::resolveModulePath(const std::string &moduleId) {
  // moduleId is like "@std.io" or "@app.util.math"
  // Parse out the mount and the rest
  if (moduleId.empty() || moduleId[0] != '@') {
    throw std::runtime_error("Invalid module ID: " + moduleId);
  }

  // Find first dot
  size_t firstDot = moduleId.find('.', 1);
  std::string mount, rest;

  if (firstDot == std::string::npos) {
    // No dots, just @mount
    mount = moduleId.substr(1); // remove @
    rest = "";
  } else {
    mount = moduleId.substr(1, firstDot - 1);
    rest = moduleId.substr(firstDot + 1);
  }

  // Determine base path
  fs::path basePath;
  if (mount == "std") {
    basePath = fs::path(stdlibRoot_);
  } else if (mount == "app") {
    basePath = fs::path(projectRoot_) / "src";
  } else {
    throw std::runtime_error("Unknown module mount '@" + mount +
                             "'. Use '@std' or '@app'.");
  }

  // Convert dotted rest to path: util.math -> util/math.lu
  fs::path modulePath = basePath;
  if (!rest.empty()) {
    std::string pathPart = rest;
    for (char &c : pathPart) {
      if (c == '.')
        c = fs::path::preferred_separator;
    }
    modulePath = basePath / (pathPart + ".lu");
  } else {
    // Just the mount with no subpath - not valid
    throw std::runtime_error("Module ID '" + moduleId +
                             "' must have at least one component after mount.");
  }

  if (!fs::exists(modulePath)) {
    throw std::runtime_error("Module file not found: " + modulePath.string() +
                             " (for module " + moduleId + ")");
  }

  return modulePath.string();
}

MapPtr Interpreter::loadModule(const std::string &moduleId) {
  // Check cache first
  auto cacheIt = moduleCache_.find(moduleId);
  if (cacheIt != moduleCache_.end()) {
    return cacheIt->second;
  }

  // Check for cyclic imports
  if (modulesLoading_.count(moduleId)) {
    throw std::runtime_error("Cyclic import detected: " + moduleId);
  }
  modulesLoading_.insert(moduleId);

  // Resolve path
  std::string modulePath = resolveModulePath(moduleId);

  // Read file
  std::ifstream in(modulePath, std::ios::in | std::ios::binary);
  if (!in) {
    modulesLoading_.erase(moduleId);
    throw std::runtime_error("Could not open module file: " + modulePath);
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string source = ss.str();

  // Lex and parse
  Lexer lexer(source);
  auto tokens = lexer.scanTokens();
  Parser parser(tokens);
  auto program = parser.parse();

  // Save current state
  auto savedEnv = env_;
  auto savedExports = currentExports_;
  auto savedModuleId = currentModuleId_;
  bool savedInModuleLoad = inModuleLoad_;

  // Set up module execution context
  env_ = std::make_shared<Environment>(globals_);
  currentExports_ = std::make_shared<LumaMap>();
  currentModuleId_ = "";
  inModuleLoad_ = true;

  try {
    // Execute the module
    for (const auto &stmt : program) {
      execute(*stmt);

      // After executing a top-level def or class with Open visibility,
      // add it to exports
      if (auto *f = dynamic_cast<const FuncDefStmt *>(stmt.get())) {
        if (f->visibility == Visibility::Open) {
          Value val = env_->get(f->name);
          currentExports_->values[f->name.lexeme] = val;
        }
      }
      if (auto *c = dynamic_cast<const ClassStmt *>(stmt.get())) {
        if (c->visibility == Visibility::Open) {
          Value val = env_->get(c->name);
          currentExports_->values[c->name.lexeme] = val;
        }
      }
    }
  } catch (...) {
    // Restore state
    env_ = savedEnv;
    currentExports_ = savedExports;
    currentModuleId_ = savedModuleId;
    inModuleLoad_ = savedInModuleLoad;
    modulesLoading_.erase(moduleId);
    throw;
  }

  // Get exports and cache
  MapPtr exports = currentExports_;
  moduleCache_[moduleId] = exports;
  moduleAstCache_[moduleId] = std::move(program); // Keep AST alive!

  // Restore state
  env_ = savedEnv;
  currentExports_ = savedExports;
  currentModuleId_ = savedModuleId;
  inModuleLoad_ = savedInModuleLoad;
  modulesLoading_.erase(moduleId);

  injectNativeNatives(moduleId, exports);

  return exports;
}

// ========== Native Implementations ==========

#include <chrono>
#include <ctime>
#include <thread>
#include <cstdlib>

static Value nativeTimeNow(const std::vector<Value> &args) {
  using namespace std::chrono;
  // Return seconds since epoch
  auto now = system_clock::now();
  auto duration = now.time_since_epoch();
  double seconds = duration_cast<milliseconds>(duration).count() / 1000.0;
  return seconds;
}

static Value nativeTimeSleep(const std::vector<Value> &args) {
    if (auto ms = std::get_if<double>(&args[0])) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*ms)));
    }
    return std::monostate{};
}

// Async module natives
static Value nativeAsyncSleep(const std::vector<Value> &args) {
    if (auto ms = std::get_if<double>(&args[0])) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*ms)));
    }
    return std::monostate{};
}

// Net module natives
static Value nativeNetIsIpv4(const std::vector<Value> &args) {
    if (auto ip = std::get_if<std::string>(&args[0])) {
        // Simple IPv4 validation
        std::regex ipv4_pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
        std::smatch matches;
        if (std::regex_match(*ip, matches, ipv4_pattern)) {
            for (size_t i = 1; i <= 4; ++i) {
                int octet = std::stoi(matches[i].str());
                if (octet < 0 || octet > 255) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static Value nativeNetIsIpv6(const std::vector<Value> &args) {
    if (auto ip = std::get_if<std::string>(&args[0])) {
        // Basic IPv6 validation
        std::regex ipv6_pattern(R"(^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::([0-9a-fA-F]{1,4}:)*[0-9a-fA-F]{1,4}$|^([0-9a-fA-F]{1,4}:)*::([0-9a-fA-F]{1,4}:)*[0-9a-fA-F]{1,4}$)");
        return std::regex_match(*ip, ipv6_pattern);
    }
    return false;
}

static Value nativeNetIpv4ToInt(const std::vector<Value> &args) {
    if (auto ip = std::get_if<std::string>(&args[0])) {
        std::istringstream iss(*ip);
        std::string octet;
        unsigned long result = 0;
        int shift = 24;

        while (std::getline(iss, octet, '.')) {
            if (shift < 0) return std::monostate{};
            int num = std::stoi(octet);
            if (num < 0 || num > 255) return std::monostate{};
            result |= (num << shift);
            shift -= 8;
        }

        return static_cast<double>(result);
    }
    return std::monostate{};
}

static Value nativeNetIntToIpv4(const std::vector<Value> &args) {
    if (auto num = std::get_if<double>(&args[0])) {
        unsigned long ip = static_cast<unsigned long>(*num);
        std::ostringstream oss;
        oss << ((ip >> 24) & 255) << "."
            << ((ip >> 16) & 255) << "."
            << ((ip >> 8) & 255) << "."
            << (ip & 255);
        return oss.str();
    }
    return std::monostate{};
}

static Value nativeNetDnsLookup(const std::vector<Value> &args) {
    // Placeholder - DNS lookup would require network libraries
    if (auto hostname = std::get_if<std::string>(&args[0])) {
        // For now, return a mock result
        auto result = std::make_shared<LumaMap>();
        auto addresses = std::make_shared<List>();
        addresses->elements.push_back(std::string("127.0.0.1"));
        result->values["addresses"] = addresses;
        result->values["type"] = 1.0; // A record
        result->values["error"] = std::monostate{};
        return result;
    }
    return std::monostate{};
}

static Value nativeNetGetHostname(const std::vector<Value> &args) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return std::string("localhost");
}

static Value nativeNetParseUrl(const std::vector<Value> &args) {
    if (auto url = std::get_if<std::string>(&args[0])) {
        auto result = std::make_shared<LumaMap>();

        // Simple URL parsing
        size_t colon_pos = url->find(':');
        if (colon_pos != std::string::npos) {
            result->values["protocol"] = url->substr(0, colon_pos);
            std::string rest = url->substr(colon_pos + 1);

            if (rest.substr(0, 2) == "//") {
                rest = rest.substr(2);
                size_t slash_pos = rest.find('/');
                std::string host_port;
                std::string path_query;

                if (slash_pos != std::string::npos) {
                    host_port = rest.substr(0, slash_pos);
                    path_query = rest.substr(slash_pos);
                } else {
                    host_port = rest;
                    path_query = "/";
                }

                size_t colon_pos2 = host_port.find(':');
                if (colon_pos2 != std::string::npos) {
                    result->values["hostname"] = host_port.substr(0, colon_pos2);
                    result->values["port"] = std::stod(host_port.substr(colon_pos2 + 1));
                } else {
                    result->values["hostname"] = host_port;
                    result->values["port"] = 80.0; // default
                }

                result->values["pathname"] = path_query;

                size_t query_pos = path_query.find('?');
                if (query_pos != std::string::npos) {
                    result->values["pathname"] = path_query.substr(0, query_pos);
                    result->values["search"] = path_query.substr(query_pos);
                } else {
                    result->values["search"] = std::string("");
                }
            }
        }

        return result;
    }
    return std::monostate{};
}

// Socket module natives
static Value nativeSocketCreate(const std::vector<Value> &args) {
    if (args.size() < 2) return std::monostate{};

    auto family = std::get_if<double>(&args[0]);
    auto type = std::get_if<double>(&args[1]);

    if (!family || !type) return std::monostate{};

    int sock_family = (*family == 2.0) ? AF_INET : AF_INET; // Default to IPv4
    int sock_type = (*type == 1.0) ? SOCK_STREAM : SOCK_DGRAM; // TCP or UDP

    int sockfd = socket(sock_family, sock_type, 0);
    if (sockfd < 0) {
        return std::monostate{};
    }

    return static_cast<double>(sockfd);
}

static Value nativeSocketBind(const std::vector<Value> &args) {
    if (args.size() < 3) return false;

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto addr_val = std::get_if<std::string>(&args[1]);
    auto port_val = std::get_if<double>(&args[2]);

    if (!sockfd_val || !addr_val || !port_val) return false;

    int sockfd = static_cast<int>(*sockfd_val);
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(*port_val));
    server_addr.sin_addr.s_addr = inet_addr(addr_val->c_str());

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }

    return true;
}

static Value nativeSocketListen(const std::vector<Value> &args) {
    if (args.size() < 2) return false;

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto backlog_val = std::get_if<double>(&args[1]);

    if (!sockfd_val || !backlog_val) return false;

    int sockfd = static_cast<int>(*sockfd_val);
    int backlog = static_cast<int>(*backlog_val);

    if (listen(sockfd, backlog) < 0) {
        return false;
    }

    return true;
}

static Value nativeSocketAccept(const std::vector<Value> &args) {
    if (args.size() < 1) return std::monostate{};

    auto sockfd_val = std::get_if<double>(&args[0]);
    if (!sockfd_val) return std::monostate{};

    int sockfd = static_cast<int>(*sockfd_val);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        return std::monostate{};
    }

    auto result = std::make_shared<LumaMap>();
    result->values["fd"] = static_cast<double>(client_fd);
    result->values["address"] = std::string(inet_ntoa(client_addr.sin_addr));
    result->values["port"] = static_cast<double>(ntohs(client_addr.sin_port));

    return result;
}

static Value nativeSocketConnect(const std::vector<Value> &args) {
    if (args.size() < 3) return false;

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto addr_val = std::get_if<std::string>(&args[1]);
    auto port_val = std::get_if<double>(&args[2]);

    if (!sockfd_val || !addr_val || !port_val) return false;

    int sockfd = static_cast<int>(*sockfd_val);
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(*port_val));

    if (inet_pton(AF_INET, addr_val->c_str(), &server_addr.sin_addr) <= 0) {
        return false;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }

    return true;
}

static Value nativeSocketSend(const std::vector<Value> &args) {
    if (args.size() < 2) return std::monostate{};

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto data_val = std::get_if<std::string>(&args[1]);

    if (!sockfd_val || !data_val) return std::monostate{};

    int sockfd = static_cast<int>(*sockfd_val);
    ssize_t sent = send(sockfd, data_val->c_str(), data_val->length(), 0);

    if (sent < 0) {
        return std::monostate{};
    }

    return static_cast<double>(sent);
}

static Value nativeSocketRecv(const std::vector<Value> &args) {
    if (args.size() < 2) return std::monostate{};

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto max_bytes_val = std::get_if<double>(&args[1]);

    if (!sockfd_val || !max_bytes_val) return std::monostate{};

    int sockfd = static_cast<int>(*sockfd_val);
    size_t max_bytes = static_cast<size_t>(*max_bytes_val);

    std::vector<char> buffer(max_bytes);
    ssize_t received = recv(sockfd, buffer.data(), max_bytes, 0);

    if (received < 0) {
        return std::monostate{};
    }

    return std::string(buffer.data(), received);
}

static Value nativeSocketSendTo(const std::vector<Value> &args) {
    if (args.size() < 4) return std::monostate{};

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto data_val = std::get_if<std::string>(&args[1]);
    auto addr_val = std::get_if<std::string>(&args[2]);
    auto port_val = std::get_if<double>(&args[3]);

    if (!sockfd_val || !data_val || !addr_val || !port_val) return std::monostate{};

    int sockfd = static_cast<int>(*sockfd_val);
    struct sockaddr_in dest_addr;

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(static_cast<uint16_t>(*port_val));

    if (inet_pton(AF_INET, addr_val->c_str(), &dest_addr.sin_addr) <= 0) {
        return std::monostate{};
    }

    ssize_t sent = sendto(sockfd, data_val->c_str(), data_val->length(), 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    if (sent < 0) {
        return std::monostate{};
    }

    return static_cast<double>(sent);
}

static Value nativeSocketRecvFrom(const std::vector<Value> &args) {
    if (args.size() < 2) return std::monostate{};

    auto sockfd_val = std::get_if<double>(&args[0]);
    auto max_bytes_val = std::get_if<double>(&args[1]);

    if (!sockfd_val || !max_bytes_val) return std::monostate{};

    int sockfd = static_cast<int>(*sockfd_val);
    size_t max_bytes = static_cast<size_t>(*max_bytes_val);

    std::vector<char> buffer(max_bytes);
    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);

    ssize_t received = recvfrom(sockfd, buffer.data(), max_bytes, 0,
                               (struct sockaddr*)&src_addr, &src_len);

    if (received < 0) {
        return std::monostate{};
    }

    auto result = std::make_shared<LumaMap>();
    result->values["data"] = std::string(buffer.data(), received);
    result->values["address"] = std::string(inet_ntoa(src_addr.sin_addr));
    result->values["port"] = static_cast<double>(ntohs(src_addr.sin_port));

    return result;
}

static Value nativeSocketClose(const std::vector<Value> &args) {
    if (args.size() < 1) return false;

    auto sockfd_val = std::get_if<double>(&args[0]);
    if (!sockfd_val) return false;

    int sockfd = static_cast<int>(*sockfd_val);
    close(sockfd);
    return true;
}

static Value nativeSocketSetOption(const std::vector<Value> &args) {
    // Placeholder implementation
    return true;
}

static Value nativeSocketGetOption(const std::vector<Value> &args) {
    // Placeholder implementation
    return std::monostate{};
}

static Value nativeOsName(const std::vector<Value> &args) {
#ifdef _WIN32
  return std::string("Windows");
#elif __APPLE__
  return std::string("macOS");
#elif __linux__
  return std::string("Linux");
#else
  return std::string("Unknown");
#endif
}

static Value nativeOsCwd(const std::vector<Value> &args) {
  return fs::current_path().string();
}

static Value nativeOsEnv(const std::vector<Value> &args) {
    if (auto key = std::get_if<std::string>(&args[0])) {
        const char* val = std::getenv(key->c_str());
        if (val) return std::string(val);
    }
    return std::monostate{};
}

static Value nativeOsExit(const std::vector<Value> &args) {
    int code = 0;
    if (!args.empty()) {
        if (auto c = std::get_if<double>(&args[0])) {
            code = static_cast<int>(*c);
        }
    }
    std::exit(code);
    return std::monostate{}; // Unreachable
}

static Value nativeIoAsk(const std::vector<Value> &args) {
    if (auto prompt = std::get_if<std::string>(&args[0])) {
        std::cout << *prompt;
        // Ensure prompt is displayed immediately
        std::cout.flush();
    }
    std::string line;
    if (std::getline(std::cin, line)) {
        return line;
    }
    return std::monostate{};
}

static Value nativeIoInput(const std::vector<Value> &args) {
    std::string line;
    if (std::getline(std::cin, line)) {
        return line;
    }
    return std::monostate{};
}

// --- JSON Helpers ---

static std::string jsonStringify(const Value &v);

static std::string jsonStringifyList(const ListPtr &list) {
  std::string s = "[";
  const auto &elems = list->elements;
  for (size_t i = 0; i < elems.size(); ++i) {
    if (i > 0)
      s += ","; // JSON standard: no space required, but usually compact
    s += jsonStringify(elems[i]);
  }
  s += "]";
  return s;
}

static std::string jsonStringifyMap(const MapPtr &map) {
  std::string s = "{";
  const auto &values = map->values;
  size_t i = 0;
  for (const auto &[key, val] : values) {
    if (i > 0)
      s += ",";
    s += "\"" + key + "\":" + jsonStringify(val);
    i++;
  }
  s += "}";
  return s;
}

static std::string jsonEscape(const std::string &str) {
  std::string result = "\"";
  for (char c : str) {
    switch (c) {
    case '\"': result += "\\\""; break;
    case '\\': result += "\\\\"; break;
    case '\b': result += "\\b"; break;
    case '\f': result += "\\f"; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default:
      if (static_cast<unsigned char>(c) < 32) {
        // control chars
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        result += buf;
      } else {
        result += c;
      }
    }
  }
  result += "\"";
  return result;
}

static std::string jsonStringify(const Value &v) {
  if (std::holds_alternative<std::monostate>(v))
    return "null";
  if (auto d = std::get_if<double>(&v)) {
    // Check if integer
    double ip;
    if (std::modf(*d, &ip) == 0.0) {
      return std::to_string(static_cast<long long>(*d));
    }
    return std::to_string(*d);
  }
  if (auto s = std::get_if<std::string>(&v))
    return jsonEscape(*s);
  if (auto b = std::get_if<bool>(&v))
    return *b ? "true" : "false";
  if (auto l = std::get_if<ListPtr>(&v))
    return jsonStringifyList(*l);
  if (auto m = std::get_if<MapPtr>(&v))
    return jsonStringifyMap(*m);
  
  return "\"<unsupported>\"";
}

static Value nativeJsonStringify(const std::vector<Value> &args) {
  return jsonStringify(args[0]);
}

// Helper for parsing
struct JsonParser {
  std::string src;
  size_t current = 0;

  JsonParser(std::string s) : src(std::move(s)) {}

  Value parse() {
    skipWhitespace();
    if (isAtEnd()) return std::monostate{};
    char c = peek();
    
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == '"') return parseString();
    if (c == '-' || isdigit(c)) return parseNumber();
    if (c == 't') return parseTrue();
    if (c == 'f') return parseFalse();
    if (c == 'n') return parseNull();

    throw std::runtime_error("Invalid JSON at position " + std::to_string(current));
  }

  Value parseObject() {
    consume('{');
    auto map = std::make_shared<LumaMap>();
    skipWhitespace();
    if (peek() == '}') {
        advance();
        return map;
    }

    while (true) {
        skipWhitespace();
        if (peek() != '"') throw std::runtime_error("Expected string key in JSON object");
        std::string key = parseStringVal();
        skipWhitespace();
        consume(':');
        Value val = parse();
        map->values[key] = val;
        
        skipWhitespace();
        if (peek() == '}') {
            advance();
            break;
        }
        consume(',');
    }
    return map;
  }

  Value parseArray() {
    consume('[');
    auto list = std::make_shared<List>();
    skipWhitespace();
    if (peek() == ']') {
        advance();
        return list;
    }

    while (true) {
        list->elements.push_back(parse());
        skipWhitespace();
        if (peek() == ']') {
            advance();
            break;
        }
        consume(',');
    }
    return list;
  }

  std::string parseStringVal() {
      consume('"');
      std::string res;
      while (!isAtEnd() && peek() != '"') {
          char c = advance();
          if (c == '\\') {
              if (isAtEnd()) throw std::runtime_error("Unterminated string escape");
              char next = advance();
              switch (next) {
                  case '"': res += '"'; break;
                  case '\\': res += '\\'; break;
                  case '/': res += '/'; break;
                  case 'b': res += '\b'; break;
                  case 'f': res += '\f'; break;
                  case 'n': res += '\n'; break;
                  case 'r': res += '\r'; break;
                  case 't': res += '\t'; break;
                  case 'u': 
                      // skip unicode implementation for brevity
                      advance(); advance(); advance(); advance(); 
                      res += '?'; 
                      break; 
                  default: res += next; break;
              }
          } else {
              res += c;
          }
      }
      consume('"');
      return res;
  }
  
  Value parseString() {
      return parseStringVal();
  }

  Value parseNumber() {
    size_t start = current;
    if (peek() == '-') advance();
    while (isdigit(peek())) advance();
    if (peek() == '.') {
        advance();
        while (isdigit(peek())) advance();
    }
    // scientific notation not supported for simplicity
    std::string numStr = src.substr(start, current - start);
    return std::stod(numStr);
  }

  Value parseTrue() {
    consume('t'); consume('r'); consume('u'); consume('e');
    return true;
  }
  Value parseFalse() {
    consume('f'); consume('a'); consume('l'); consume('s'); consume('e');
    return false;
  }
  Value parseNull() {
    consume('n'); consume('u'); consume('l'); consume('l');
    return std::monostate{};
  }

  void consume(char c) {
    if (peek() == c) {
        advance();
    } else {
        throw std::runtime_error("Expected '" + std::string(1, c) + "'");
    }
  }

  void skipWhitespace() {
    while (!isAtEnd() && isspace(peek())) advance();
  }

  char peek() {
    if (isAtEnd()) return '\0';
    return src[current];
  }

  char advance() {
    if (isAtEnd()) return '\0';
    return src[current++];
  }

  bool isAtEnd() { return current >= src.size(); }
};

static Value nativeJsonParse(const std::vector<Value> &args) {
  if (auto s = std::get_if<std::string>(&args[0])) {
    try {
        JsonParser parser(*s);
        return parser.parse();
    } catch (const std::exception& e) {
        return std::monostate{}; // return nil on error
    }
  }
  return std::monostate{};
}

// ========== Math Natives ==========
static Value nativeMathSqrt(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::sqrt(*x);
    return std::monostate{};
}
static Value nativeMathSin(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::sin(*x);
    return std::monostate{};
}
static Value nativeMathCos(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::cos(*x);
    return std::monostate{};
}
static Value nativeMathTan(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::tan(*x);
    return std::monostate{};
}
static Value nativeMathAbs(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::abs(*x);
    return std::monostate{};
}
static Value nativeMathCeil(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::ceil(*x);
    return std::monostate{};
}
static Value nativeMathFloor(const std::vector<Value> &args) {
    if (auto x = std::get_if<double>(&args[0])) return std::floor(*x);
    return std::monostate{};
}
static Value nativeMathPi(const std::vector<Value> &args) {
    return 3.141592653589793;
}

static std::string trimString(const std::string &input) {
  size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
    start++;
  }
  if (start == input.size()) return "";

  size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    end--;
  }

  return input.substr(start, end - start);
}

static double requireNumberValue(const Value &v, const std::string &where) {
  if (auto n = std::get_if<double>(&v)) return *n;
  throw std::runtime_error("Expected number in " + where + ".");
}

static std::string requireStringValue(const Value &v, const std::string &where) {
  if (auto s = std::get_if<std::string>(&v)) return *s;
  throw std::runtime_error("Expected string in " + where + ".");
}

static std::mt19937 &globalRng();

static Value nativeFsExists(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "fs.exists path");
  return fs::exists(path);
}

static Value nativeFsIsDir(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "fs.is_dir path");
  return fs::is_directory(path);
}

static Value nativeFsReadFile(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "fs.read_file path");
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return std::monostate{};

  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

static Value nativeFsWriteFile(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "fs.write_file path");
  std::string data = requireStringValue(args[1], "fs.write_file data");
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) return false;
  file << data;
  return file.good();
}

static Value nativeFsListDir(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "fs.list_dir path");
  auto list = std::make_shared<List>();
  try {
    for (const auto &entry : fs::directory_iterator(path)) {
      list->elements.push_back(entry.path().filename().string());
    }
  } catch (const std::exception &) {
    return std::monostate{};
  }
  return list;
}

static std::string shellQuote(const std::string &arg) {
  std::string quoted = "'";
  for (char c : arg) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

static Value runShellCapture(const std::string &command) {
  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) return std::monostate{};

  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }
  int rc = pclose(pipe);
  if (rc != 0) return std::monostate{};
  return output;
}

static Value nativeHttpGet(const std::vector<Value> &args) {
  std::string url = requireStringValue(args[0], "http.get url");
  std::string command = "curl -fsSL --max-time 10 " + shellQuote(url) + " 2>/dev/null";
  return runShellCapture(command);
}

static Value nativeHttpPost(const std::vector<Value> &args) {
  std::string url = requireStringValue(args[0], "http.post url");
  std::string body = requireStringValue(args[1], "http.post body");
  std::string command = "curl -fsSL --max-time 10 -X POST --data-binary " + shellQuote(body) + " " + shellQuote(url) + " 2>/dev/null";
  return runShellCapture(command);
}

static std::string hexFromUint64(uint64_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(16) << value;
  return oss.str();
}

static std::string pseudoSha256(const std::string &data) {
  static const std::array<uint64_t, 4> salts = {
      0x9e3779b97f4a7c15ULL, 0xc2b2ae3d27d4eb4fULL,
      0x165667b19e3779f9ULL, 0xd6e8feb86659fd93ULL};

  std::hash<std::string> hasher;
  std::string digest;
  digest.reserve(64);

  for (auto salt : salts) {
    digest += hexFromUint64(hasher(data + std::to_string(salt)) ^ salt);
  }
  return digest;
}

static Value nativeCryptoHash(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "crypto.hash data");
  return pseudoSha256(data);
}

static Value nativeCryptoRandomBytes(const std::vector<Value> &args) {
  double requested = requireNumberValue(args[0], "crypto.random_bytes length");
  if (requested < 0) {
    throw std::runtime_error("crypto.random_bytes length cannot be negative.");
  }

  size_t length = static_cast<size_t>(requested);
  std::uniform_int_distribution<int> dist(0, 255);
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < length; ++i) {
    int byte = dist(globalRng());
    oss << std::setw(2) << byte;
  }
  return oss.str();
}

static Value nativeRegexMatch(const std::vector<Value> &args) {
  std::string pattern = requireStringValue(args[0], "regex.match pattern");
  std::string text = requireStringValue(args[1], "regex.match text");
  try {
    std::regex re(pattern);
    return std::regex_match(text, re);
  } catch (const std::regex_error &e) {
    throw std::runtime_error(std::string("Invalid regex: ") + e.what());
  }
}

static Value nativeRegexSearch(const std::vector<Value> &args) {
  std::string pattern = requireStringValue(args[0], "regex.search pattern");
  std::string text = requireStringValue(args[1], "regex.search text");
  try {
    std::regex re(pattern);
    return std::regex_search(text, re);
  } catch (const std::regex_error &e) {
    throw std::runtime_error(std::string("Invalid regex: ") + e.what());
  }
}

static Value nativeRegexReplace(const std::vector<Value> &args) {
  std::string pattern = requireStringValue(args[0], "regex.replace pattern");
  std::string text = requireStringValue(args[1], "regex.replace text");
  std::string replacement = requireStringValue(args[2], "regex.replace replacement");
  try {
    std::regex re(pattern);
    return std::regex_replace(text, re, replacement);
  } catch (const std::regex_error &e) {
    throw std::runtime_error(std::string("Invalid regex: ") + e.what());
  }
}

static Value nativeRegexSplit(const std::vector<Value> &args) {
  std::string pattern = requireStringValue(args[0], "regex.split pattern");
  std::string text = requireStringValue(args[1], "regex.split text");
  auto list = std::make_shared<List>();
  try {
    std::regex re(pattern);
    std::sregex_token_iterator it(text.begin(), text.end(), re, -1);
    std::sregex_token_iterator end;
    for (; it != end; ++it) {
      list->elements.push_back(it->str());
    }
  } catch (const std::regex_error &e) {
    throw std::runtime_error(std::string("Invalid regex: ") + e.what());
  }
  return list;
}

static Value nativeStringUpper(const std::vector<Value> &args) {
  std::string value = requireStringValue(args[0], "string.upper");
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::toupper(c); });
  return value;
}

static Value nativeStringLower(const std::vector<Value> &args) {
  std::string value = requireStringValue(args[0], "string.lower");
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value;
}

static Value nativeStringTrim(const std::vector<Value> &args) {
  std::string value = requireStringValue(args[0], "string.trim");
  return trimString(value);
}

static Value nativeStringStartsWith(const std::vector<Value> &args) {
  std::string value = requireStringValue(args[0], "string.starts_with value");
  std::string prefix = requireStringValue(args[1], "string.starts_with prefix");
  if (prefix.size() > value.size()) return false;
  return value.compare(0, prefix.size(), prefix) == 0;
}

static Value nativeStringEndsWith(const std::vector<Value> &args) {
  std::string value = requireStringValue(args[0], "string.ends_with value");
  std::string suffix = requireStringValue(args[1], "string.ends_with suffix");
  if (suffix.size() > value.size()) return false;
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static Value nativeStringSplit(const std::vector<Value> &args) {
  std::string value = requireStringValue(args[0], "string.split value");
  std::string delim = requireStringValue(args[1], "string.split delimiter");
  if (delim.empty()) {
    throw std::runtime_error("Delimiter cannot be empty in string.split.");
  }

  auto list = std::make_shared<List>();
  size_t start = 0;
  while (true) {
    size_t pos = value.find(delim, start);
    if (pos == std::string::npos) {
      list->elements.push_back(value.substr(start));
      break;
    }
    list->elements.push_back(value.substr(start, pos - start));
    start = pos + delim.size();
  }
  return list;
}

static Value nativeStringJoin(const std::vector<Value> &args) {
  const Value &listVal = args[0];
  std::string delim = requireStringValue(args[1], "string.join delimiter");

  auto listPtr = std::get_if<ListPtr>(&listVal);
  if (!listPtr) {
    throw std::runtime_error("Expected list in string.join.");
  }

  std::ostringstream out;
  const auto &elements = (*listPtr)->elements;
  for (size_t i = 0; i < elements.size(); ++i) {
    if (i > 0) out << delim;
    out << requireStringValue(elements[i], "string.join elements");
  }
  return out.str();
}

static std::mt19937 &globalRng() {
  static std::mt19937 engine(std::random_device{}());
  return engine;
}

static Value nativeRandomNumber(const std::vector<Value> &args) {
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  return dist(globalRng());
}

static Value nativeRandomBetween(const std::vector<Value> &args) {
  double min = requireNumberValue(args[0], "random.between min");
  double max = requireNumberValue(args[1], "random.between max");
  if (max < min) std::swap(min, max);
  std::uniform_real_distribution<double> dist(min, max);
  return dist(globalRng());
}

static Value nativeRandomInt(const std::vector<Value> &args) {
  double min = requireNumberValue(args[0], "random.int min");
  double max = requireNumberValue(args[1], "random.int max");
  if (max < min) std::swap(min, max);
  std::uniform_int_distribution<int> dist(static_cast<int>(std::floor(min)), static_cast<int>(std::floor(max)));
  return static_cast<double>(dist(globalRng()));
}

// ========== Path Module Natives ==========

static Value nativePathJoin(const std::vector<Value> &args) {
  std::string result;
  for (size_t i = 0; i < args.size(); ++i) {
    std::string part = requireStringValue(args[i], "path.join part");
    if (!result.empty() && !part.empty()) {
      result += "/";
    }
    result += part;
  }
  return result;
}

static Value nativePathDirname(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.dirname path");
  size_t last_sep = path.find_last_of('/');
  if (last_sep == std::string::npos) {
    return ".";
  }
  if (last_sep == 0) {
    return "/";
  }
  return path.substr(0, last_sep);
}

static Value nativePathBasename(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.basename path");
  size_t last_sep = path.find_last_of('/');
  if (last_sep == std::string::npos) {
    return path;
  }
  return path.substr(last_sep + 1);
}

static Value nativePathExtname(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.extname path");
  size_t last_dot = path.find_last_of('.');
  size_t last_sep = path.find_last_of('/');
  if (last_dot == std::string::npos || (last_sep != std::string::npos && last_dot < last_sep)) {
    return "";
  }
  return path.substr(last_dot);
}

static Value nativePathStem(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.stem path");
  size_t last_dot = path.find_last_of('.');
  size_t last_sep = path.find_last_of('/');
  if (last_dot == std::string::npos || (last_sep != std::string::npos && last_dot < last_sep)) {
    return nativePathBasename(args);
  }
  std::string basename = std::get<std::string>(nativePathBasename(args));
  return basename.substr(0, last_dot - (path.length() - basename.length()));
}

static Value nativePathNormalize(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.normalize path");
  // Simple normalization - remove duplicate slashes and resolve . and ..
  // This is a basic implementation
  return path;
}

static Value nativePathAbsolute(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.absolute path");
  if (path[0] == '/') {
    return path;
  }
  // In a real implementation, this would get the current working directory
  return "/" + path;
}

static Value nativePathIsAbsolute(const std::vector<Value> &args) {
  std::string path = requireStringValue(args[0], "path.is_absolute path");
  return path[0] == '/';
}

static Value nativePathRelative(const std::vector<Value> &args) {
  std::string from = requireStringValue(args[0], "path.relative from");
  std::string to = requireStringValue(args[1], "path.relative to");
  // Simple implementation
  return to;
}

static Value nativePathResolve(const std::vector<Value> &args) {
  // Simple implementation - just return the last argument
  if (args.empty()) return "";
  return requireStringValue(args.back(), "path.resolve path");
}

static Value nativePathSep(const std::vector<Value> &args) {
  return "/";
}

static Value nativePathDelimiter(const std::vector<Value> &args) {
  return ":";
}

// ========== Encoding Module Natives ==========

static Value nativeEncodingBase64Encode(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.base64_encode data");
  // Basic base64 implementation would go here
  // For now, return the input unchanged
  return data;
}

static Value nativeEncodingBase64Decode(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.base64_decode data");
  // Basic base64 decode implementation would go here
  return data;
}

static Value nativeEncodingUrlEncode(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.url_encode data");
  // URL encoding implementation would go here
  return data;
}

static Value nativeEncodingUrlDecode(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.url_decode data");
  // URL decoding implementation would go here
  return data;
}

static Value nativeEncodingHexEncode(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.hex_encode data");
  std::string result;
  for (char c : data) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", (unsigned char)c);
    result += buf;
  }
  return result;
}

static Value nativeEncodingHexDecode(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.hex_decode data");
  std::string result;
  for (size_t i = 0; i < data.length(); i += 2) {
    std::string byte_str = data.substr(i, 2);
    char byte = (char)strtol(byte_str.c_str(), nullptr, 16);
    result += byte;
  }
  return result;
}

static Value nativeEncodingHtmlEscape(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.html_escape data");
  std::string result;
  for (char c : data) {
    switch (c) {
      case '&': result += "&amp;"; break;
      case '<': result += "&lt;"; break;
      case '>': result += "&gt;"; break;
      case '"': result += "&quot;"; break;
      case '\'': result += "&#x27;"; break;
      default: result += c; break;
    }
  }
  return result;
}

static Value nativeEncodingHtmlUnescape(const std::vector<Value> &args) {
  std::string data = requireStringValue(args[0], "encoding.html_unescape data");
  // Simple implementation - replace common entities
  std::string result = data;
  // Replace in reverse order to avoid conflicts
  size_t pos;
  while ((pos = result.find("&amp;")) != std::string::npos) result.replace(pos, 5, "&");
  while ((pos = result.find("&lt;")) != std::string::npos) result.replace(pos, 4, "<");
  while ((pos = result.find("&gt;")) != std::string::npos) result.replace(pos, 4, ">");
  while ((pos = result.find("&quot;")) != std::string::npos) result.replace(pos, 6, "\"");
  return result;
}

// ========== DateTime Module Natives ==========

static Value nativeDateTimeNow(const std::vector<Value> &args) {
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  return static_cast<double>(timestamp);
}

static Value nativeDateTimeParse(const std::vector<Value> &args) {
  // Basic implementation - would need proper date parsing
  return std::monostate{};
}

static Value nativeDateTimeFromComponents(const std::vector<Value> &args) {
  // Would convert components to timestamp
  return static_cast<double>(0);
}

static Value nativeDateTimeYear(const std::vector<Value> &args) {
  double timestamp = requireNumberValue(args[0], "datetime.year timestamp");
  time_t t = static_cast<time_t>(timestamp);
  struct tm *tm = gmtime(&t);
  return static_cast<double>(tm->tm_year + 1900);
}

static Value nativeDateTimeMonth(const std::vector<Value> &args) {
  double timestamp = requireNumberValue(args[0], "datetime.month timestamp");
  time_t t = static_cast<time_t>(timestamp);
  struct tm *tm = gmtime(&t);
  return static_cast<double>(tm->tm_mon + 1);
}

static Value nativeDateTimeDay(const std::vector<Value> &args) {
  double timestamp = requireNumberValue(args[0], "datetime.day timestamp");
  time_t t = static_cast<time_t>(timestamp);
  struct tm *tm = gmtime(&t);
  return static_cast<double>(tm->tm_mday);
}

static Value nativeDateTimeFormat(const std::vector<Value> &args) {
  double timestamp = requireNumberValue(args[0], "datetime.format timestamp");
  std::string format = requireStringValue(args[1], "datetime.format format");
  time_t t = static_cast<time_t>(timestamp);
  struct tm *tm = gmtime(&t);
  char buffer[100];
  strftime(buffer, sizeof(buffer), format.c_str(), tm);
  return std::string(buffer);
}

static Value nativeDateTimeDayOfWeek(const std::vector<Value> &args) {
  double timestamp = requireNumberValue(args[0], "datetime.day_of_week timestamp");
  time_t t = static_cast<time_t>(timestamp);
  struct tm *tm = gmtime(&t);
  return static_cast<double>(tm->tm_wday);
}

static Value nativeDateTimeDayOfYear(const std::vector<Value> &args) {
  double timestamp = requireNumberValue(args[0], "datetime.day_of_year timestamp");
  time_t t = static_cast<time_t>(timestamp);
  struct tm *tm = gmtime(&t);
  return static_cast<double>(tm->tm_yday + 1);
}

static Value nativeDateTimeIsLeapYear(const std::vector<Value> &args) {
  double year = requireNumberValue(args[0], "datetime.is_leap_year year");
  int y = static_cast<int>(year);
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static Value nativeDateTimeDaysInMonth(const std::vector<Value> &args) {
  double year = requireNumberValue(args[0], "datetime.days_in_month year");
  double month = requireNumberValue(args[1], "datetime.days_in_month month");
  int y = static_cast<int>(year);
  int m = static_cast<int>(month);
  int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) {
    return static_cast<double>(29);
  }
  return static_cast<double>(days[m - 1]);
}

// ========== Sys Module Natives ==========

static Value nativeSysPlatform(const std::vector<Value> &args) {
#ifdef _WIN32
  return "win32";
#elif __APPLE__
  return "darwin";
#elif __linux__
  return "linux";
#else
  return "unknown";
#endif
}

static Value nativeSysArch(const std::vector<Value> &args) {
#ifdef __x86_64__
  return "x64";
#elif __i386__
  return "ia32";
#elif __arm__
  return "arm";
#elif __aarch64__
  return "arm64";
#else
  return "unknown";
#endif
}

static Value nativeSysPlatformInfo(const std::vector<Value> &args) {
  auto info = std::make_shared<LumaMap>();
  info->values["os"] = nativeSysPlatform(args);
  info->values["arch"] = nativeSysArch(args);
  info->values["version"] = "unknown";
  return info;
}

static Value nativeSysCpuCount(const std::vector<Value> &args) {
  return static_cast<double>(std::thread::hardware_concurrency());
}

static Value nativeSysCpuInfo(const std::vector<Value> &args) {
  auto info = std::make_shared<LumaMap>();
  info->values["count"] = nativeSysCpuCount(args);
  info->values["model"] = "unknown";
  return info;
}

static Value nativeSysTotalMemory(const std::vector<Value> &args) {
  // Platform-specific memory detection would go here
  return static_cast<double>(0);
}

static Value nativeSysAvailableMemory(const std::vector<Value> &args) {
  // Platform-specific memory detection would go here
  return static_cast<double>(0);
}

static Value nativeSysMemoryInfo(const std::vector<Value> &args) {
  auto info = std::make_shared<LumaMap>();
  double total = std::get<double>(nativeSysTotalMemory(args));
  double available = std::get<double>(nativeSysAvailableMemory(args));
  info->values["total"] = total;
  info->values["available"] = available;
  info->values["used"] = total - available;
  return info;
}

static Value nativeSysPid(const std::vector<Value> &args) {
  return static_cast<double>(getpid());
}

static Value nativeSysPpid(const std::vector<Value> &args) {
  return static_cast<double>(getppid());
}

static Value nativeSysProcessInfo(const std::vector<Value> &args) {
  auto info = std::make_shared<LumaMap>();
  info->values["pid"] = nativeSysPid(args);
  info->values["ppid"] = nativeSysPpid(args);
  info->values["command"] = "unknown";
  return info;
}

static Value nativeSysLoadAverage(const std::vector<Value> &args) {
  auto list = std::make_shared<List>();
  list->elements = {static_cast<double>(0), static_cast<double>(0), static_cast<double>(0)};
  return list;
}

static Value nativeSysUptime(const std::vector<Value> &args) {
  return static_cast<double>(0);
}

static Value nativeSysHostname(const std::vector<Value> &args) {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return std::string(hostname);
  }
  return "unknown";
}

static Value nativeSysNetworkInterfaces(const std::vector<Value> &args) {
  auto list = std::make_shared<List>();
  // Network interface detection would go here
  return list;
}

static Value nativeSysExecutablePath(const std::vector<Value> &args) {
  // Would need platform-specific implementation
  return "";
}

static Value nativeSysCwd(const std::vector<Value> &args) {
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    return std::string(cwd);
  }
  return "";
}

static Value nativeSysEnviron(const std::vector<Value> &args) {
  auto env = std::make_shared<LumaMap>();
  extern char **environ;
  for (char **envp = environ; *envp != nullptr; ++envp) {
    std::string env_var = *envp;
    size_t equals_pos = env_var.find('=');
    if (equals_pos != std::string::npos) {
      std::string key = env_var.substr(0, equals_pos);
      std::string value = env_var.substr(equals_pos + 1);
      env->values[key] = value;
    }
  }
  return env;
}

static Value nativeSysExit(const std::vector<Value> &args) {
  double code = requireNumberValue(args[0], "sys.exit code");
  exit(static_cast<int>(code));
  return std::monostate{}; // Unreachable
}

static Value nativeSysArgv(const std::vector<Value> &args) {
  // Would need to store argv from main
  auto list = std::make_shared<List>();
  return list;
}

static Value nativeSysProgname(const std::vector<Value> &args) {
  return "luma";
}

// ========== UUID Module Natives ==========

static Value nativeUuidV4(const std::vector<Value> &args) {
  // Simple UUID v4 implementation
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::string uuid = "";
  for (int i = 0; i < 32; ++i) {
    if (i == 8 || i == 12 || i == 16 || i == 20) {
      uuid += "-";
    }
    int r = dis(gen);
    char c = (r < 10) ? ('0' + r) : ('a' + r - 10);
    uuid += c;
  }
  return uuid;
}

static Value nativeUuidNil(const std::vector<Value> &args) {
  return "00000000-0000-0000-0000-000000000000";
}

static Value nativeUuidIsValid(const std::vector<Value> &args) {
  std::string uuid = requireStringValue(args[0], "uuid.is_valid uuid");
  if (uuid.length() != 36) return false;

  // Check format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  for (size_t i = 0; i < uuid.length(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-') return false;
    } else {
      char c = uuid[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        return false;
      }
    }
  }
  return true;
}

static Value nativeUuidParse(const std::vector<Value> &args) {
  std::string uuid = requireStringValue(args[0], "uuid.parse uuid");
  if (!std::get<bool>(nativeUuidIsValid(args))) {
    return std::monostate{};
  }

  auto parsed = std::make_shared<LumaMap>();
  parsed->values["string"] = uuid;
  parsed->values["version"] = static_cast<double>(4); // Assume v4
  parsed->values["variant"] = static_cast<double>(1); // RFC 4122
  return parsed;
}

static Value nativeUuidStringify(const std::vector<Value> &args) {
  if (std::holds_alternative<std::monostate>(args[0])) {
    return nativeUuidNil(args);
  }
  auto uuid_obj = std::get<MapPtr>(args[0]);
  return std::get<std::string>(uuid_obj->values["string"]);
}

// ========== URL Module Natives ==========

static Value nativeUrlParse(const std::vector<Value> &args) {
  std::string url_str = requireStringValue(args[0], "url.parse url");

  auto parsed = std::make_shared<LumaMap>();
  parsed->values["href"] = url_str;
  parsed->values["protocol"] = "";
  parsed->values["hostname"] = "";
  parsed->values["port"] = "";
  parsed->values["pathname"] = "";
  parsed->values["search"] = "";
  parsed->values["hash"] = "";

  // Simple URL parsing
  size_t proto_end = url_str.find("://");
  if (proto_end != std::string::npos) {
    parsed->values["protocol"] = url_str.substr(0, proto_end);
    std::string rest = url_str.substr(proto_end + 3);

    size_t path_start = rest.find('/');
    size_t query_start = rest.find('?');
    size_t hash_start = rest.find('#');

    size_t host_end = std::min({path_start, query_start, hash_start, rest.length()});
    std::string host_part = rest.substr(0, host_end);

    // Parse hostname and port
    size_t colon_pos = host_part.find(':');
    if (colon_pos != std::string::npos) {
      parsed->values["hostname"] = host_part.substr(0, colon_pos);
      parsed->values["port"] = host_part.substr(colon_pos + 1);
    } else {
      parsed->values["hostname"] = host_part;
    }

    if (path_start != std::string::npos && path_start < rest.length()) {
      size_t path_end = std::min({query_start, hash_start, rest.length()});
      if (path_end == std::string::npos) path_end = rest.length();
      parsed->values["pathname"] = rest.substr(path_start, path_end - path_start);
    }

    if (query_start != std::string::npos && query_start < rest.length()) {
      size_t query_end = (hash_start != std::string::npos) ? hash_start : rest.length();
      parsed->values["search"] = rest.substr(query_start, query_end - query_start);
    }

    if (hash_start != std::string::npos && hash_start < rest.length()) {
      parsed->values["hash"] = rest.substr(hash_start);
    }
  }

  return parsed;
}

static Value nativeUrlFormat(const std::vector<Value> &args) {
  auto url_obj = std::get<MapPtr>(args[0]);
  std::string result;

  std::string protocol = std::get<std::string>(url_obj->values["protocol"]);
  if (!protocol.empty()) {
    result += protocol;
    if (protocol.back() != ':') result += ":";
    if (result.back() != '/') result += "//";
  }

  std::string hostname = std::get<std::string>(url_obj->values["hostname"]);
  if (!hostname.empty()) {
    result += hostname;
  }

  std::string port = std::get<std::string>(url_obj->values["port"]);
  if (!port.empty()) {
    result += ":" + port;
  }

  std::string pathname = std::get<std::string>(url_obj->values["pathname"]);
  if (!pathname.empty()) {
    result += pathname;
  }

  std::string search = std::get<std::string>(url_obj->values["search"]);
  if (!search.empty()) {
    result += search;
  }

  std::string hash = std::get<std::string>(url_obj->values["hash"]);
  if (!hash.empty()) {
    result += hash;
  }

  return result;
}

static Value nativeUrlParseQuery(const std::vector<Value> &args) {
  std::string query_str = requireStringValue(args[0], "url.parse_query query");

  auto params = std::make_shared<LumaMap>();

  if (query_str.empty()) {
    return params;
  }

  // Remove leading ?
  if (query_str[0] == '?') {
    query_str = query_str.substr(1);
  }

  size_t pos = 0;
  while (pos < query_str.length()) {
    size_t amp_pos = query_str.find('&', pos);
    if (amp_pos == std::string::npos) {
      amp_pos = query_str.length();
    }

    std::string pair = query_str.substr(pos, amp_pos - pos);
    size_t eq_pos = pair.find('=');

    std::string key, value;
    if (eq_pos != std::string::npos) {
      key = pair.substr(0, eq_pos);
      value = pair.substr(eq_pos + 1);
    } else {
      key = pair;
      value = "";
    }

    // URL decode (simplified)
    params->values[key] = value;

    pos = amp_pos + 1;
  }

  return params;
}

static Value nativeUrlBuildQuery(const std::vector<Value> &args) {
  auto params = std::get<MapPtr>(args[0]);
  std::string result;

  bool first = true;
  for (const auto& [key, value] : params->values) {
    if (!first) result += "&";
    result += key + "=" + valueToString(value);
    first = false;
  }

  if (!result.empty()) {
    result = "?" + result;
  }

  return result;
}

static Value nativeUrlResolve(const std::vector<Value> &args) {
  std::string from = requireStringValue(args[0], "url.resolve from");
  std::string to = requireStringValue(args[1], "url.resolve to");

  // Simple implementation
  if (to.find("://") != std::string::npos) {
    return to; // Absolute URL
  }

  return to; // For now, just return 'to'
}


void Interpreter::injectNativeNatives(const std::string &moduleId, MapPtr exports) {
  auto defineNative = [&](const std::string &name, std::function<Value(const std::vector<Value>&)> func, size_t arity) {
      auto native = std::make_shared<NativeFunctionObject>();
      native->name = name;
      native->func = func;
      native->arity = arity;
      native->variadic = false;
      exports->values[name] = native;
  };

  if (moduleId == "@std.time") {
    defineNative("now", nativeTimeNow, 0);
    defineNative("sleep", nativeTimeSleep, 1);
  } else if (moduleId == "@std.os") {
    defineNative("name", nativeOsName, 0);
    defineNative("cwd", nativeOsCwd, 0);
    defineNative("env", nativeOsEnv, 1);
    defineNative("exit", nativeOsExit, 1);
  } else if (moduleId == "@std.json") {
      defineNative("stringify", nativeJsonStringify, 1);
      defineNative("parse", nativeJsonParse, 1);
  } else if (moduleId == "@std.io") {
      defineNative("input", nativeIoInput, 0);
      defineNative("ask", nativeIoAsk, 1);
  } else if (moduleId == "@std.math") {
      defineNative("sqrt", nativeMathSqrt, 1);
      defineNative("sin", nativeMathSin, 1);
      defineNative("cos", nativeMathCos, 1);
      defineNative("tan", nativeMathTan, 1);
      defineNative("abs", nativeMathAbs, 1);
      defineNative("ceil", nativeMathCeil, 1);
      defineNative("floor", nativeMathFloor, 1);
      defineNative("pi", nativeMathPi, 0);
  } else if (moduleId == "@std.string") {
      defineNative("upper", nativeStringUpper, 1);
      defineNative("lower", nativeStringLower, 1);
      defineNative("trim", nativeStringTrim, 1);
      defineNative("starts_with", nativeStringStartsWith, 2);
      defineNative("ends_with", nativeStringEndsWith, 2);
      defineNative("split", nativeStringSplit, 2);
      defineNative("join", nativeStringJoin, 2);
  } else if (moduleId == "@std.random") {
      defineNative("number", nativeRandomNumber, 0);
      defineNative("between", nativeRandomBetween, 2);
      defineNative("int", nativeRandomInt, 2);
  } else if (moduleId == "@std.fs") {
      defineNative("exists", nativeFsExists, 1);
      defineNative("is_dir", nativeFsIsDir, 1);
      defineNative("read_file", nativeFsReadFile, 1);
      defineNative("write_file", nativeFsWriteFile, 2);
      defineNative("list_dir", nativeFsListDir, 1);
  } else if (moduleId == "@std.http") {
      defineNative("get", nativeHttpGet, 1);
      defineNative("post", nativeHttpPost, 2);
  } else if (moduleId == "@std.crypto") {
      defineNative("hash", nativeCryptoHash, 1);
      defineNative("random_bytes", nativeCryptoRandomBytes, 1);
  } else if (moduleId == "@std.regex") {
      defineNative("match", nativeRegexMatch, 2);
      defineNative("search", nativeRegexSearch, 2);
      defineNative("replace", nativeRegexReplace, 3);
      defineNative("split", nativeRegexSplit, 2);
  } else if (moduleId == "@std.path") {
      defineNative("join", nativePathJoin, 1); // variadic
      defineNative("dirname", nativePathDirname, 1);
      defineNative("basename", nativePathBasename, 1);
      defineNative("extname", nativePathExtname, 1);
      defineNative("stem", nativePathStem, 1);
      defineNative("normalize", nativePathNormalize, 1);
      defineNative("absolute", nativePathAbsolute, 1);
      defineNative("is_absolute", nativePathIsAbsolute, 1);
      defineNative("relative", nativePathRelative, 2);
      defineNative("resolve", nativePathResolve, 1); // variadic
      defineNative("sep", nativePathSep, 0);
      defineNative("delimiter", nativePathDelimiter, 0);
  } else if (moduleId == "@std.encoding") {
      defineNative("base64_encode", nativeEncodingBase64Encode, 1);
      defineNative("base64_decode", nativeEncodingBase64Decode, 1);
      defineNative("url_encode", nativeEncodingUrlEncode, 1);
      defineNative("url_decode", nativeEncodingUrlDecode, 1);
      defineNative("hex_encode", nativeEncodingHexEncode, 1);
      defineNative("hex_decode", nativeEncodingHexDecode, 1);
      defineNative("html_escape", nativeEncodingHtmlEscape, 1);
      defineNative("html_unescape", nativeEncodingHtmlUnescape, 1);
  } else if (moduleId == "@std.datetime") {
      defineNative("now", nativeDateTimeNow, 0);
      defineNative("parse", nativeDateTimeParse, 2);
      defineNative("from_components", nativeDateTimeFromComponents, 6);
      defineNative("year", nativeDateTimeYear, 1);
      defineNative("month", nativeDateTimeMonth, 1);
      defineNative("day", nativeDateTimeDay, 1);
      defineNative("format", nativeDateTimeFormat, 2);
      defineNative("day_of_week", nativeDateTimeDayOfWeek, 1);
      defineNative("day_of_year", nativeDateTimeDayOfYear, 1);
      defineNative("is_leap_year", nativeDateTimeIsLeapYear, 1);
      defineNative("days_in_month", nativeDateTimeDaysInMonth, 2);
  } else if (moduleId == "@std.sys") {
      defineNative("platform", nativeSysPlatform, 0);
      defineNative("arch", nativeSysArch, 0);
      defineNative("platform_info", nativeSysPlatformInfo, 0);
      defineNative("cpu_count", nativeSysCpuCount, 0);
      defineNative("cpu_info", nativeSysCpuInfo, 0);
      defineNative("total_memory", nativeSysTotalMemory, 0);
      defineNative("available_memory", nativeSysAvailableMemory, 0);
      defineNative("memory_info", nativeSysMemoryInfo, 0);
      defineNative("pid", nativeSysPid, 0);
      defineNative("ppid", nativeSysPpid, 0);
      defineNative("process_info", nativeSysProcessInfo, 0);
      defineNative("load_average", nativeSysLoadAverage, 0);
      defineNative("uptime", nativeSysUptime, 0);
      defineNative("hostname", nativeSysHostname, 0);
      defineNative("network_interfaces", nativeSysNetworkInterfaces, 0);
      defineNative("executable_path", nativeSysExecutablePath, 0);
      defineNative("cwd", nativeSysCwd, 0);
      defineNative("environ", nativeSysEnviron, 0);
      defineNative("exit", nativeSysExit, 1);
      defineNative("argv", nativeSysArgv, 0);
      defineNative("progname", nativeSysProgname, 0);
  } else if (moduleId == "@std.uuid") {
      defineNative("v4", nativeUuidV4, 0);
      defineNative("nil", nativeUuidNil, 0);
      defineNative("is_valid", nativeUuidIsValid, 1);
      defineNative("parse", nativeUuidParse, 1);
      defineNative("stringify", nativeUuidStringify, 1);
  } else if (moduleId == "@std.url") {
      defineNative("parse", nativeUrlParse, 1);
      defineNative("format", nativeUrlFormat, 1);
      defineNative("parse_query", nativeUrlParseQuery, 1);
      defineNative("build_query", nativeUrlBuildQuery, 1);
      defineNative("resolve", nativeUrlResolve, 2);
  } else if (moduleId == "@std.async") {
      defineNative("sleep", nativeAsyncSleep, 1);
  } else if (moduleId == "@std.net") {
      defineNative("is_ipv4", nativeNetIsIpv4, 1);
      defineNative("is_ipv6", nativeNetIsIpv6, 1);
      defineNative("ipv4_to_int", nativeNetIpv4ToInt, 1);
      defineNative("int_to_ipv4", nativeNetIntToIpv4, 1);
      defineNative("dns_lookup", nativeNetDnsLookup, 1);
      defineNative("get_hostname", nativeNetGetHostname, 0);
      defineNative("parse_url", nativeNetParseUrl, 1);
  } else if (moduleId == "@std.socket") {
      defineNative("create", nativeSocketCreate, 2);
      defineNative("bind", nativeSocketBind, 3);
      defineNative("listen", nativeSocketListen, 2);
      defineNative("accept", nativeSocketAccept, 1);
      defineNative("connect", nativeSocketConnect, 3);
      defineNative("send", nativeSocketSend, 2);
      defineNative("recv", nativeSocketRecv, 2);
      defineNative("sendto", nativeSocketSendTo, 4);
      defineNative("recvfrom", nativeSocketRecvFrom, 2);
      defineNative("close", nativeSocketClose, 1);
      defineNative("set_option", nativeSocketSetOption, 3);
      defineNative("get_option", nativeSocketGetOption, 2);
  }
}

// ========== Global Native Implementations ==========

static Value nativeLen(const std::vector<Value> &args) {
  const Value &v = args[0];
  if (auto l = std::get_if<ListPtr>(&v))
    return (double)(*l)->elements.size();
  if (auto m = std::get_if<MapPtr>(&v))
    return (double)(*m)->values.size();
  if (auto s = std::get_if<std::string>(&v))
    return (double)s->length();
  throw std::runtime_error("Object has no length (only list, map, string).");
}

static Value nativePush(const std::vector<Value> &args) {
  if (auto l = std::get_if<ListPtr>(&args[0])) {
    (*l)->elements.push_back(args[1]);
    return args[1]; // return pushed value
  }
  throw std::runtime_error("Expected list for push.");
}

static Value nativePop(const std::vector<Value> &args) {
  if (auto l = std::get_if<ListPtr>(&args[0])) {
    if ((*l)->elements.empty())
      return std::monostate{}; // return nil
    Value v = (*l)->elements.back();
    (*l)->elements.pop_back();
    return v;
  }
  throw std::runtime_error("Expected list for pop.");
}

static Value nativeKeys(const std::vector<Value> &args) {
  if (auto m = std::get_if<MapPtr>(&args[0])) {
    auto list = std::make_shared<List>();
    for (const auto &[k, v] : (*m)->values) {
      list->elements.push_back(k);
    }
    return list;
  }
  throw std::runtime_error("Expected map for keys.");
}

static Value nativeRemove(const std::vector<Value> &args) {
  if (auto m = std::get_if<MapPtr>(&args[0])) {
    if (auto k = std::get_if<std::string>(&args[1])) {
      if ((*m)->values.count(*k)) {
        Value v = (*m)->values.at(*k);
        (*m)->values.erase(*k);
        return v;
      }
      return std::monostate{};
    }
    throw std::runtime_error("Map keys must be strings.");
  }
  throw std::runtime_error("Expected map for remove.");
}
