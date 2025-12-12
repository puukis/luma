#include "ast_printer.hpp"
#include <sstream>

std::string AstPrinter::ind() const { return std::string(indent_ * 2, ' '); }

std::string AstPrinter::print(const std::vector<StmtPtr> &program) {
  std::ostringstream out;
  for (const auto &s : program) {
    out << stmtToString(*s);
  }
  return out.str();
}

std::string AstPrinter::stmtToString(const Stmt &s) {
  std::ostringstream out;

  if (auto *c = dynamic_cast<const ClassStmt *>(&s)) {
    out << ind() << "class " << c->name.lexeme << " {\n";
    indent_++;
    for (const auto &m : c->methods) {
      // reuse FuncDefStmt logic but need to handle shared_ptr
      // FuncDefStmt logic:
      out << ind() << "def " << m->name.lexeme << "(";
      for (size_t k = 0; k < m->params.size(); k++) {
        if (k)
          out << ", ";
        out << m->params[k].lexeme;
      }
      out << ") ";
      out << stmtToString(*m->body);
    }
    indent_--;
    out << ind() << "}\n";
    return out.str();
  }

  if (auto *x = dynamic_cast<const ExprStmt *>(&s)) {
    out << ind() << exprToString(*x->expr) << "\n";
    return out.str();
  }

  if (auto *p = dynamic_cast<const PrintStmt *>(&s)) {
    out << ind() << "print(" << exprToString(*p->expr) << ")\n";
    return out.str();
  }

  if (auto *a = dynamic_cast<const VarAssignStmt *>(&s)) {
    out << ind() << a->name.lexeme << " = " << exprToString(*a->value) << "\n";
    return out.str();
  }

  if (auto *b = dynamic_cast<const BlockStmt *>(&s)) {
    out << ind() << "{\n";
    indent_++;
    for (const auto &st : b->statements)
      out << stmtToString(*st);
    indent_--;
    out << ind() << "}\n";
    return out.str();
  }

  if (auto *i = dynamic_cast<const IfStmt *>(&s)) {
    out << ind() << "if (" << exprToString(*i->condition) << ") ";
    out << stmtToString(*i->thenBranch);
    if (i->elseBranch) {
      out << ind() << "else " << stmtToString(*i->elseBranch);
    }
    return out.str();
  }

  if (auto *w = dynamic_cast<const WhileStmt *>(&s)) {
    out << ind() << "while (" << exprToString(*w->condition) << ") ";
    out << stmtToString(*w->body);
    return out.str();
  }

  if (auto *u = dynamic_cast<const UntilStmt *>(&s)) {
    out << ind() << "until (" << exprToString(*u->condition) << ") ";
    out << stmtToString(*u->body);
    return out.str();
  }

  if (auto *r = dynamic_cast<const ReturnStmt *>(&s)) {

    out << ind() << "return";
    if (r->value)
      out << " " << exprToString(*r->value);
    out << "\n";
    return out.str();
  }

  if (auto *f = dynamic_cast<const FuncDefStmt *>(&s)) {
    out << ind();
    if (f->visibility == Visibility::Open)
      out << "open ";
    out << "def " << f->name.lexeme << "(";
    for (size_t k = 0; k < f->params.size(); k++) {
      if (k)
        out << ", ";
      out << f->params[k].lexeme;
    }
    out << ") ";
    out << stmtToString(*f->body);
    return out.str();
  }

  // ========== Luma Unique Statements ==========

  if (auto *e = dynamic_cast<const EchoStmt *>(&s)) {
    out << ind() << "echo " << exprToString(*e->count) << " ";
    out << stmtToString(*e->body);
    return out.str();
  }

  if (auto *sw = dynamic_cast<const SwapStmt *>(&s)) {
    out << ind() << sw->left.lexeme << " <-> " << sw->right.lexeme << "\n";
    return out.str();
  }

  if (auto *m = dynamic_cast<const MaybeStmt *>(&s)) {
    out << ind() << "maybe ";
    out << stmtToString(*m->tryBlock);
    if (m->otherwiseBlock) {
      out << ind() << "otherwise ";
      out << stmtToString(*m->otherwiseBlock);
    }
    return out.str();
  }

  // ========== Module System Statements ==========

  if (auto *mod = dynamic_cast<const ModuleStmt *>(&s)) {
    out << ind() << "module ";
    for (const auto &t : mod->moduleIdParts) {
      out << t.lexeme;
    }
    out << "\n";
    return out.str();
  }

  if (auto *use = dynamic_cast<const UseStmt *>(&s)) {
    out << ind() << "use ";
    for (const auto &t : use->moduleIdParts) {
      out << t.lexeme;
    }
    out << " as " << use->alias.lexeme << "\n";
    return out.str();
  }

  out << ind() << "<unknown stmt>\n";
  return out.str();
}

std::string AstPrinter::exprToString(const Expr &e) {
  std::ostringstream out;

  if (auto *l = dynamic_cast<const LiteralExpr *>(&e)) {
    switch (l->kind) {
    case LiteralExpr::Kind::Number:
      out << l->numberValue;
      break;
    case LiteralExpr::Kind::String:
      out << "\"" << l->stringValue << "\"";
      break;
    case LiteralExpr::Kind::Bool:
      out << (l->boolValue ? "true" : "false");
      break;
    case LiteralExpr::Kind::Nil:
      out << "nil";
      break;
    }
    return out.str();
  }

  if (auto *v = dynamic_cast<const VariableExpr *>(&e)) {
    return v->name.lexeme;
  }

  if (auto *g = dynamic_cast<const GroupingExpr *>(&e)) {
    out << "(" << exprToString(*g->expr) << ")";
    return out.str();
  }

  if (auto *u = dynamic_cast<const UnaryExpr *>(&e)) {
    out << "(" << u->op.lexeme << exprToString(*u->right) << ")";
    return out.str();
  }

  if (auto *b = dynamic_cast<const BinaryExpr *>(&e)) {
    out << "(" << exprToString(*b->left) << " " << b->op.lexeme << " "
        << exprToString(*b->right) << ")";
    return out.str();
  }

  if (auto *c = dynamic_cast<const CallExpr *>(&e)) {
    out << exprToString(*c->callee) << "(";
    for (size_t k = 0; k < c->args.size(); k++) {
      if (k)
        out << ", ";
      out << exprToString(*c->args[k]);
    }
    out << ")";
    return out.str();
  }

  if (auto *l = dynamic_cast<const ListExpr *>(&e)) {
    out << "[";
    for (size_t k = 0; k < l->elements.size(); k++) {
      if (k)
        out << ", ";
      out << exprToString(*l->elements[k]);
    }
    out << "]";
    return out.str();
  }

  if (auto *g = dynamic_cast<const GetExpr *>(&e)) {
    out << exprToString(*g->object) << "." << g->name.lexeme;
    return out.str();
  }

  if (auto *i = dynamic_cast<const IndexExpr *>(&e)) {
    out << exprToString(*i->object) << "[" << exprToString(*i->index) << "]";
    return out.str();
  }

  if (auto *s = dynamic_cast<const SetExpr *>(&e)) {
    // SetExpr is usually an expression in AST? Or only stmt?
    // It's an Expr.
    out << exprToString(*s->object) << "." << s->name.lexeme << " = "
        << exprToString(*s->value);
    return out.str();
  }

  if (auto *t = dynamic_cast<const ThisExpr *>(&e)) {
    return "this";
  }

  if (auto *m = dynamic_cast<const MapExpr *>(&e)) {
    out << "{";
    for (size_t k = 0; k < m->keys.size(); k++) {
      if (k > 0)
        out << ", ";
      out << exprToString(*m->keys[k]) << " = " << exprToString(*m->values[k]);
    }
    out << "}";
    return out.str();
  }

  return "<unknown expr>";
}
