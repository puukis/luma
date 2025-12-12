#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

Interpreter::Interpreter() {
  globals_ = std::make_shared<Environment>();
  env_ = globals_;
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
  }
}
