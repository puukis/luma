#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.hpp"
#include "environment.hpp"
#include "value.hpp"

class Interpreter {
public:
  Interpreter();

  void run(const std::vector<StmtPtr> &program);

  // Set the entry file path (used to determine project root)
  void setExecutablePath(const std::string &path);
  void setEntryFile(const std::string &path);

private:
  std::shared_ptr<Environment> globals_;
  std::shared_ptr<Environment> env_;

  // Module system
  std::unordered_map<std::string, MapPtr> moduleCache_;
  std::unordered_map<std::string, std::vector<StmtPtr>> moduleAstCache_; // Keep ASTs alive
  std::unordered_set<std::string> modulesLoading_;
  std::string entryFilePath_;
  std::string executablePath_;
  std::string projectRoot_;
  std::string stdlibRoot_;
  std::string currentModuleId_;
  bool inModuleLoad_ = false;
  MapPtr currentExports_;

  // Return signal used internally
  struct ReturnSignal {
    Value value;
  };

  void execute(const Stmt &stmt);
  Value evaluate(const Expr &expr);

  void executeBlock(const BlockStmt &block,
                    std::shared_ptr<Environment> newEnv);

  // helpers
  void assignOrDefine(const Token &name, Value value);
  void visitClassStmt(const ClassStmt &stmt);

  Value callFunction(const Value &callee, const std::vector<Value> &args,
                     const Token &callSiteParen);

  // Module system helpers
  MapPtr loadModule(const std::string &moduleId);
  std::string resolveModulePath(const std::string &moduleId);
  std::string moduleIdToString(const std::vector<Token> &parts);
  void initializeRoots();
};
