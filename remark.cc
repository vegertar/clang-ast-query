#include "remark.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/TextNodeDumper.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Tooling/Syntax/Tokens.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>

#include <optional>
#include <unistd.h>

namespace {

using namespace clang;

class expanded_decl {
public:
  expanded_decl(SourceLocation loc, const Decl *decl)
      : loc(loc), decl(decl), prev(nullptr) {
    assert(loc.isMacroID());
  }

  SourceLocation get_location() const { return loc; }

  const Decl *get_decl() const { return decl; }

  expanded_decl *get_previous() const { return prev; }

  void set_previous(expanded_decl *prev) {
    if (prev->get_decl() == get_decl()) {
      this->prev = prev->get_previous();
      delete prev;
    } else {
      this->prev = prev;
    }
  }

private:
  SourceLocation loc;
  const Decl *decl;
  expanded_decl *prev;
};

class index_value_t {
public:
  explicit index_value_t(const MacroInfo *p) : data((void *)p) {
    static_assert(alignof(MacroInfo) >= 4 && alignof(MacroInfo) % 4 == 0);
    assert(is_macro() && get_raw() == p);
  }

  explicit index_value_t(const Decl *p) : data((char *)p + 1) {
    static_assert(alignof(Decl) >= 4 && alignof(Decl) % 4 == 0);
    assert(is_decl() && get_raw() == p);
  }

  explicit index_value_t(expanded_decl *p) : data((char *)p + 2) {
    static_assert(alignof(expanded_decl) >= 4 &&
                  alignof(expanded_decl) % 4 == 0);
    assert(is_expanded_decl() && get_raw() == p);
  }

  int kind() const { return ((std::uintptr_t)data & mask); }

  bool is_macro() const { return kind() == 0; }

  bool is_decl() const { return kind() == 1; }

  bool is_expanded_decl() const { return kind() == 2; }

  const MacroInfo *get_macro() const {
    assert(is_macro());
    return (const MacroInfo *)get_raw();
  }

  const Decl *get_decl() const {
    assert(is_decl());
    return (const Decl *)get_raw();
  }

  expanded_decl *get_expanded_decl() const {
    assert(is_expanded_decl());
    return (expanded_decl *)get_raw();
  }

  void *get_raw() const { return (void *)((std::uintptr_t)data & ~mask); }

  bool operator==(index_value_t other) const {
    return get_raw() == other.get_raw();
  }

private:
  void *data;
  static const std::uintptr_t mask = 3;
};

class raw_line_ostream : public raw_ostream {
public:
  explicit raw_line_ostream(int (*parse)(char *line, size_t n, size_t cap,
                                         void *data) = nullptr,
                            void *data = nullptr, size_t cap = BUFSIZ)
      : pos(0), parse(parse), data(data), escaped(0) {
    line.reserve(cap);
    SetUnbuffered();
  }

  void escape(char c) { escaped = c; }

  void write_impl(const char *ptr, size_t size) override {
    if (!escaped)
      return write_private(ptr, size);

    size_t i = 0, j = 0;
    do {
      while (j < size && ptr[j] != escaped)
        ++j;

      write_private(ptr + i, j - i);
      if (j == size)
        break;

      write_private("\\", 1);
      i = j++;
    } while (i < size);
  }

  uint64_t current_pos() const override { return pos; }

private:
  void write_private(const char *ptr, size_t size) {
    pos += size;

    size_t i = 0;
    while (i < size) {
      if (ptr[i++] == '\n') {
        // with 2 more slots to work with parse_line()
        size_t cap = line.size() + i + 2;
        if (line.capacity() < cap)
          line.reserve(cap);
        line.append(ptr, i);
        if (parse)
          parse(line.data(), line.size(), line.capacity(), data);

        ptr += i;
        size -= i;
        i = 0;
        line.clear();
      }
    }

    if (size)
      line.append(ptr, size);
  }

  std::string line;
  uint64_t pos;
  int (*parse)(char *line, size_t n, size_t cap, void *data);
  void *data;
  char escaped;
};

class ast_consumer final : public ASTConsumer {
public:
  ast_consumer(std::unique_ptr<raw_line_ostream> os, Preprocessor &pp)
      : out(*os), pp(pp), os(std::move(os)), visitor(*this),
        last_macro_expansion(0, 0), dir(0) {
    directive_nodes.emplace_back();
  }

protected:
  void Initialize(ASTContext &context) override {
    dumper.emplace(out, context, false);
    pp.addPPCallbacks(std::make_unique<pp_callback>(*this));
    pp.setTokenWatcher([this](auto &token) { on_token_lexed(token); });
  }

  void HandleTranslationUnit(ASTContext &ctx) override {
    dump_preprocessor_directives();
    auto &sm = ctx.getSourceManager();
    const auto file = sm.getFileEntryRefForID(sm.getMainFileID());
    const auto &filename = file->getName();
    char cwd[PATH_MAX];

    out << "#TU:" << filename << '\n';
    out << "#CWD:" << getcwd(cwd, sizeof(cwd)) << '\n';

    visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    dump_token_expansion();
  }

private:
  class ast_visitor : public RecursiveASTVisitor<ast_visitor> {
  public:
    explicit ast_visitor(ast_consumer &ast) : ast(ast) {}

    bool VisitTypedefDecl(const TypedefDecl *d) {
      auto t = d->getUnderlyingType().getTypePtr();
      if (!t->getAs<TagType>()) {
        auto p = named(t);
        if (p && !p->isImplicit()) {
          index_named(p, get_location(d->getTypeSourceInfo()->getTypeLoc()));
        }
      }
      return index_named(d);
    }

    bool VisitFieldDecl(const FieldDecl *d) { return index_valuable(d); }

    bool VisitTagDecl(const TagDecl *d) {
      auto def = d->getDefinition();
      return index_named(def ? def : d, d->getLocation());
    }

    bool VisitVarDecl(const VarDecl *d) {
      if (auto p = dyn_cast<ParmVarDecl>(d)) {
        // It still needs to index the parameter type.
        // e.g. typedef void foo_t(Type parm_var)
        if (p->getDeclContext()->getDeclKind() != Decl::Function)
          index_valuable(d, true);

        return true;
      }
      return index_valuable(d);
    }

    bool VisitFunctionDecl(const FunctionDecl *d) {
      index_valuable(d);

      auto index_type_only = !d->doesThisDeclarationHaveABody();
      for (auto param : d->parameters())
        index_valuable(param, index_type_only);

      return true;
    }

    bool VisitDeclRefExpr(const DeclRefExpr *e) {
      return index_named(e->getDecl(), e->getLocation());
    }

    bool VisitMemberExpr(const MemberExpr *e) {
      return index_named(e->getMemberDecl(), e->getMemberLoc());
    }

    bool TraverseMemberExpr(MemberExpr *e) {
      // The MemberExpr is traversed in post-order, i.e. a.b.c is in c>b>a,
      // therefore use_post_order temporarily.
      ++use_post_order;
      RecursiveASTVisitor::TraverseMemberExpr(e);
      --use_post_order;
      return true;
    }

    bool shouldTraversePostOrder() const { return use_post_order; }

  private:
    bool index_named(const NamedDecl *d, SourceLocation loc = {}) {
      if (!d->getDeclName().isEmpty())
        index(loc.isValid() ? loc : d->getLocation(), d);

      return true;
    }

    SourceLocation get_location(TypeLoc cur) {
      TypeLoc left_most = cur;
      while (!cur.isNull()) {
        if (cur.getLocalSourceRange().getBegin().isValid())
          left_most = cur;
        cur = cur.getNextTypeLoc();
      }

      return left_most.getLocalSourceRange().getBegin();
    }

    template <typename T>
    bool index_valuable(const T *p, bool type_only = false) {
      if (auto d = named(p->getType().getTypePtr()); d && !d->isImplicit()) {
        if (!d->getDeclName().isEmpty()) {
          if constexpr (std::is_same_v<T, FunctionDecl>) {
            auto range = p->getReturnTypeSourceRange();
            index(range.getBegin(), d);
          } else if (auto ti = p->getTypeSourceInfo()) {
            auto type_loc = ti->getTypeLoc();
            index(get_location(type_loc), d);
          } else {
            assert(p->getLocation().isInvalid());
          }
        }
      }

      if (!type_only)
        index_named(p);

      return true;
    }

    NamedDecl *named(const Type *p) {
      if (auto t = p->getAs<TypedefType>())
        return t->getDecl();

      if (auto t = p->getAs<TagType>())
        return t->getDecl();

      if (auto t = p->getAs<FunctionType>())
        return named(t->getReturnType().getTypePtr());

      if (auto t = p->getAs<PointerType>())
        return named(t->getPointeeType().getTypePtr());

      if (auto t = p->getArrayElementTypeNoTypeQual())
        return named(t);

      return nullptr;
    }

    void index(SourceLocation loc, index_value_t val) { ast.index(loc, val); }

    void index(SourceLocation loc, const Decl *p) {
      auto &sm = ast.pp.getSourceManager();
      if (loc.isMacroID()) {
        index(sm.getExpansionLoc(loc),
              index_value_t(new expanded_decl(loc, p)));
      } else if (loc.isValid()) {
        index(loc, index_value_t(p));
      }
    }

    ast_consumer &ast;
    unsigned use_post_order = 0;
  };

  class pp_callback : public PPCallbacks {
  public:
    explicit pp_callback(ast_consumer &ast) : ast(ast) {}

    void MacroDefined(const Token &token, const MacroDirective *md) override {
      ast.directives.emplace_back(directive_def{token.getIdentifierInfo(), md});
      ast.add_directive();
    }

    void MacroExpands(const Token &token, const MacroDefinition &def,
                      SourceRange range, const MacroArgs *args) override {
      auto &expanding_stack = ast.expanding_stack;
      auto &macros = ast.macros;

      int parent = expanding_stack.empty() ? -1 : expanding_stack.back();
      unsigned remote = macros.size();
      macros.emplace_back(token, def.getMacroInfo(), range, parent, remote);
      expanding_stack.push_back(remote);

      // Update remote field up to the root
      while (parent != -1) {
        auto &macro = macros[parent];
        macro.remote = remote;
        parent = macro.parent;
      }
    }

    void MacroExpanded(const MacroInfo *mi, bool fast) override {
      assert(!ast.expanding_stack.empty() && "Unpaired macro expansion");
      auto top = ast.expanding_stack.back();
      assert(ast.macros[top].info == mi);
      ast.macros[top].fast = fast;
      ast.expanding_stack.pop_back();
    }

    void If(SourceLocation loc, SourceRange range,
            ConditionValueKind value) override {
      ast.directives.emplace_back(directive_if{loc, range, value});
      add_if_directive();
    }

    void Elif(SourceLocation loc, SourceRange range, ConditionValueKind value,
              SourceLocation if_loc) override {
      lift_if_directive(if_loc);
      ast.directives.emplace_back(directive_elif{loc, range, value, if_loc});
      add_if_directive();
    }

    void Else(SourceLocation loc, SourceLocation if_loc) override {
      lift_if_directive(if_loc);
      ast.directives.emplace_back(directive_else{loc, if_loc});
      add_if_directive();
    }

    void Endif(SourceLocation loc, SourceLocation if_loc) override {
      lift_if_directive(if_loc);
      ast.directives.emplace_back(directive_endif{loc, if_loc});
      ast.add_directive();
    }

    void Ifdef(SourceLocation loc, const Token &token,
               const MacroDefinition &md) override {
      ast.directives.emplace_back(
          directive_ifdef{loc, token.getIdentifierInfo(), md.getMacroInfo()});
      add_if_directive();
    }

    void Ifndef(SourceLocation loc, const Token &token,
                const MacroDefinition &md) override {
      ast.directives.emplace_back(
          directive_ifndef{loc, token.getIdentifierInfo(), md.getMacroInfo()});
      add_if_directive();
    }

    void Elifdef(SourceLocation loc, const Token &token,
                 const MacroDefinition &md) override {
      ast.lift_directive();
      ast.directives.emplace_back(directive_elifdef_taken{
          loc, token.getIdentifierInfo(), md.getMacroInfo()});
      add_if_directive();
    }

    void Elifdef(SourceLocation loc, SourceRange range,
                 SourceLocation if_loc) override {
      lift_if_directive(if_loc);
      ast.directives.emplace_back(directive_elifdef_skiped{loc, range, if_loc});
      add_if_directive();
    }

    void Elifndef(SourceLocation loc, const Token &token,
                  const MacroDefinition &md) override {
      ast.lift_directive();
      ast.directives.emplace_back(directive_elifndef_taken{
          loc, token.getIdentifierInfo(), md.getMacroInfo()});
      add_if_directive();
    }

    void Elifndef(SourceLocation loc, SourceRange range,
                  SourceLocation if_loc) override {
      lift_if_directive(if_loc);
      ast.directives.emplace_back(
          directive_elifndef_skiped{loc, range, if_loc});
      add_if_directive();
    }

  private:
    void lift_if_directive(SourceLocation if_loc) {
      std::visit(
          [if_loc, this](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, directive_if>) {
              assert(arg.loc == if_loc);
            } else if constexpr (std::is_same_v<T, directive_elif>) {
              assert(arg.if_loc == if_loc);
            } else if constexpr (std::is_same_v<T, directive_else>) {
              assert(arg.if_loc == if_loc);
            } else if constexpr (std::is_same_v<T, directive_endif>) {
              assert(arg.if_loc == if_loc);
            } else if constexpr (std::is_same_v<T, directive_elifdef_skiped>) {
              assert(arg.if_loc == if_loc);
            } else if constexpr (std::is_same_v<T, directive_elifndef_skiped>) {
              assert(arg.if_loc == if_loc);
            } else {
              assert("Never reach here");
            }
          },
          ast.directives[ast.directive_nodes[ast.dir].i]);
      ast.lift_directive();
    }

    void add_if_directive() {
      ast.add_directive();
      ast.dir = ast.directive_nodes[ast.dir].children.back();
      ast.make_macro_expansion();
    }

    ast_consumer &ast;
  };

  struct macro_expansion_node {
    Token token;
    MacroInfo *info;
    SourceRange range;
    int parent;
    unsigned remote;
    bool fast;
  };

  struct macro_full_expansion_node {
    unsigned i;
    bool token;
    llvm::SmallVector<macro_full_expansion_node *, 4> children;
  };

  struct directive_def {
    const IdentifierInfo *id;
    const MacroDirective *md;
  };

  struct directive_if {
    SourceLocation loc;
    SourceRange range;
    pp_callback::ConditionValueKind value;
  };

  struct directive_elif {
    SourceLocation loc;
    SourceRange range;
    pp_callback::ConditionValueKind value;
    SourceLocation if_loc;
  };

  struct directive_else {
    SourceLocation loc;
    SourceLocation if_loc;
  };

  struct directive_endif {
    SourceLocation loc;
    SourceLocation if_loc;
  };

  struct directive_ifdef {
    SourceLocation loc;
    const IdentifierInfo *id;
    const MacroInfo *mi;
  };

  struct directive_ifndef {
    SourceLocation loc;
    const IdentifierInfo *id;
    const MacroInfo *mi;
  };

  struct directive_elifdef_taken {
    SourceLocation loc;
    const IdentifierInfo *id;
    const MacroInfo *mi;
  };

  struct directive_elifdef_skiped {
    SourceLocation loc;
    SourceRange range;
    SourceLocation if_loc;
  };

  struct directive_elifndef_taken {
    SourceLocation loc;
    const IdentifierInfo *id;
    const MacroInfo *mi;
  };

  struct directive_elifndef_skiped {
    SourceLocation loc;
    SourceRange range;
    SourceLocation if_loc;
  };

  struct pseudo_directive_macro {
    macro_full_expansion_node root;
    std::vector<macro_full_expansion_node> pool;

    pseudo_directive_macro() = default;
    pseudo_directive_macro(pseudo_directive_macro &&) = default;
    pseudo_directive_macro(const pseudo_directive_macro &) = delete;
  };

  struct directive_node {
    unsigned i;      // index of directives
    unsigned parent; // index of directive_nodes
    llvm::SmallVector<unsigned, 4> children;
  };

  void dump_kind(MacroDirective::Kind kind) const {
    switch (kind) {
    case MacroDirective::MD_Define:
      out << "Def";
      break;
    case MacroDirective::MD_Undefine:
      out << "Undef";
      break;
    case MacroDirective::MD_Visibility:
      out << "Visibility";
      break;
    }
  }

  void dump_macro(const MacroInfo *mi) {
    out << "MacroInfo";
    dumper->dumpPointer(mi);
    dumper->dumpSourceRange(
        {mi->getDefinitionLoc(), mi->getDefinitionEndLoc()});

    out << " '";
    if (mi->isFunctionLike()) {
      out << '(';
      auto i = mi->param_begin(), e = mi->param_end();

      if (!mi->param_empty()) {
        for (; i + 1 != e; ++i) {
          out << (*i)->getName();
          out << ',';
        }

        // Last argument.
        if ((*i)->getName() == "__VA_ARGS__")
          out << "...";
        else
          out << (*i)->getName();
      }

      if (mi->isGNUVarargs())
        out << "..."; // foo(x...)
      out << ')';
    }
    out << "' '";
    out.escape('\'');
    if (!mi->tokens_empty()) {
      auto i = mi->tokens_begin(), e = mi->tokens_end();
      dump_token_content(*i++);

      if (pp.getSourceManager().isWrittenInBuiltinFile(
              mi->getDefinitionLoc())) {
        while (i != e) {
          if (i->hasLeadingSpace())
            out << ' ';
          dump_token_content(*i++);
        }
      } else if (i != e) {
        // Don't dump user defined macro content completely
        out << "...";
      }
    }
    out.escape(0);
    out << "'";
  }

  void on_token_lexed(const Token &token) {
    // Collect all tokens to dump in later.
    if (!token.isAnnotation())
      tokens.push_back(syntax::Token(token));

    auto loc = token.getLocation();
    if (loc.isFileID()) {
      make_macro_expansion();
    } else {
      // The fast-expanded token is expanded at the parent macro, in this case
      // the expanding_stack will be empty in advance.
      expansions.emplace_back(expanding_stack.empty() ? macros.size() - 1
                                                      : expanding_stack.back(),
                              token);
    }
  }

  bool indexable(const syntax::Token &token) {
    switch (token.kind()) {
    case tok::identifier:
      return true;
    default:
      return false;
    }
  }

  void dump_preprocessor_directives() {
    dumper->AddChild([this] {
      out << "Preprocessor";
      dumper->dumpPointer(&pp);
      for (auto child : directive_nodes.front().children) {
        dump_directive(directive_nodes[child]);
      }
    });
  }

  void dump_directive(const directive_node &node) {
    dumper->AddChild([=, this] {
      std::visit(
          [this](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, directive_def>) {
              auto md = arg.md;
              dump_kind(md->getKind());
              out << "Directive";
              dumper->dumpPointer(md);
              if (auto prev = md->getPrevious()) {
                out << " prev";
                dumper->dumpPointer(prev);
              }
              out << ' ';
              dumper->dumpLocation(md->getLocation());
              out << ' ';
              out << arg.id->getName();
              dumper->AddChild([md, this] { dump_macro(md->getMacroInfo()); });
            } else if constexpr (std::is_same_v<T, directive_if> ||
                                 std::is_same_v<T, directive_elif>) {
              if constexpr (std::is_same_v<T, directive_if>)
                out << "IfDirective";
              else if constexpr (std::is_same_v<T, directive_elif>)
                out << "ElifDirective";
              else
                assert("Never reach here");

              // Use a null pointer to compatible with the scanner.
              out << " 0x0";
              dumper->dumpSourceRange(arg.range);
              out << ' ';
              dumper->dumpLocation(arg.loc);
              out << ' ';
              switch (arg.value) {
              case pp_callback::CVK_NotEvaluated:
                out << "NotEvaluated";
                break;
              case pp_callback::CVK_False:
                out << "False";
                break;
              case pp_callback::CVK_True:
                out << "True";
                break;
              }
            } else if constexpr (std::is_same_v<T, directive_else>) {
              out << "ElseDirective 0x0";
              dumper->dumpSourceRange({arg.loc, arg.if_loc});
            } else if constexpr (std::is_same_v<T, directive_endif>) {
              out << "EndifDirective 0x0";
              dumper->dumpSourceRange({arg.loc, arg.if_loc});
            } else if constexpr (std::is_same_v<T, directive_ifdef> ||
                                 std::is_same_v<T, directive_elifdef_taken> ||
                                 std::is_same_v<T, directive_ifndef> ||
                                 std::is_same_v<T, directive_elifndef_taken>) {
              if constexpr (std::is_same_v<T, directive_ifdef>)
                out << "IfdefDirective";
              else if constexpr (std::is_same_v<T, directive_elifdef_taken>)
                out << "ElifdefDirective";
              else if constexpr (std::is_same_v<T, directive_ifndef>)
                out << "IfndefDirective";
              else if constexpr (std::is_same_v<T, directive_elifndef_taken>)
                out << "ElifndefDirective";
              else
                assert("Never reach here");

              // Use a null pointer to compatible with the scanner.
              out << " 0x0 ";
              dumper->dumpLocation(arg.loc);
              out << ' ' << arg.id->getName();
              auto mi = arg.mi;
              dumper->AddChild([mi, this] {
                out << "MacroInfo";
                dumper->dumpPointer(mi);
              });
            } else if constexpr (std::is_same_v<T, directive_elifdef_skiped> ||
                                 std::is_same_v<T, directive_elifndef_skiped>) {
              if constexpr (std::is_same_v<T, directive_elifdef_skiped>)
                out << "ElifdefDirective";
              else if constexpr (std::is_same_v<T, directive_elifndef_skiped>)
                out << "ElifndefDirective";
              else
                assert("Never reach here");

              // Use a null pointer to compatible with the scanner.
              out << " 0x0";
              dumper->dumpSourceRange(arg.range);
              out << ' ';
              dumper->dumpLocation(arg.loc);
            } else if constexpr (std::is_same_v<T, pseudo_directive_macro>) {
              out << "PseudoMacroDirective 0x0";
              for (auto child : arg.root.children) {
                dump_full_macro_expansion(*child);
              }
            } else {
              assert("Never reach here");
            }
          },
          directives[node.i]);

      for (auto child : node.children) {
        dump_directive(directive_nodes[child]);
      }
    });
  }

  void dump_token_expansion() {
    assert(!tokens.empty());
    assert(tokens.back().kind() == tok::eof);

    auto &sm = pp.getSourceManager();
    SourceLocation last_loc;
    SourceLocation last_expansion_loc;
    unsigned index_of_last_expansion_loc = -1;
    unsigned indexable_tokens = 0;
    unsigned indexed_tokens = 0;

    for (unsigned i = 0, e = tokens.size(); i < e; ++i) {
      auto loc = tokens[i].location();
      if (indexable(tokens[i])) {
        ++indexable_tokens;

        if (loc.isFileID()) {
          auto v = find(loc, true);

          out << "#TOK-DECL:";
          dumper->dumpLocation(tokens[i].location());

          if (auto d = v ? v->get_decl() : nullptr) {
            ++indexed_tokens;
            out << ' ' << d << '\n';
          } else {
            out << " 0x0\n";
          }
        }
      }

      auto expansion_loc = sm.getExpansionLoc(loc);
      if (expansion_loc != last_expansion_loc) {
        if (last_loc.isMacroID()) {
          auto v = find(last_expansion_loc, true);
          auto d = v ? v->get_expanded_decl() : nullptr;
          unsigned j = i - 1;
          bool started = false;

          while (d && j > index_of_last_expansion_loc) {
            auto &t = tokens[j];
            if (indexable(t)) {
              if (!started) {
                started = true;
                out << "#TOK-DECL:";
                dumper->dumpLocation(last_expansion_loc);
                // Starting with zero-offset and NULL decl.
                out << " 0 0x0\n";
              }

              out << "#TOK-DECL:";
              dumper->dumpLocation(t.location());
              out << ' ' << j - index_of_last_expansion_loc << ' ';

              if (t.location() == d->get_location()) {
                ++indexed_tokens;
                out << d->get_decl() << '\n';
                auto prev = d->get_previous();
                delete d;
                d = prev;
              } else {
                out << "0x0\n";
              }
            }
            --j;
          }

          // if (d) {
          //   out << "=====================================\n";
          //   dumper->dumpLocation(last_expansion_loc);
          //   out << ' ';
          //   dumper->dumpLocation(d->get_location());
          //   out << '\n';
          //   out << '\n';
          // }

          assert(!d && "All expanded_decls are visited");
        }

        last_loc = loc;
        last_expansion_loc = expansion_loc;
        index_of_last_expansion_loc = i - 1;
      }
    }

    out << "## tokens(total/indexable/indexed):" << tokens.size() << '/'
        << indexable_tokens;
    if (indexable_tokens)
      out << '(' << (indexable_tokens * 100 / tokens.size()) << "%)";
    out << '/' << indexed_tokens;
    if (indexed_tokens)
      out << '(' << (indexed_tokens * 100 / indexable_tokens) << "%)";
    out << '\n';
  }

  void lift_directive() { dir = directive_nodes[dir].parent; }

  void add_directive() {
    directive_nodes.emplace_back(directives.size() - 1, dir);
    directive_nodes[dir].children.emplace_back(directive_nodes.size() - 1);
  }

  void make_macro_expansion() {
    assert(expanding_stack.empty());

    unsigned k = last_macro_expansion.second;
    pseudo_directive_macro m;
    m.pool.reserve(macros.size() - last_macro_expansion.first +
                   expansions.size() - last_macro_expansion.second);

    index_macro(last_macro_expansion.first, macros.size());
    make_macro_full_expansion(-1, last_macro_expansion.first, macros.size(), k,
                              m.root, m.pool);
    assert(m.pool.size() == macros.size() - last_macro_expansion.first +
                                expansions.size() -
                                last_macro_expansion.second);

    if (!m.pool.empty()) {
      directives.emplace_back(std::move(m));
      last_macro_expansion.first = macros.size();
      last_macro_expansion.second = expansions.size();
      add_directive();
    }
  }

  void make_macro_full_expansion(int parent, unsigned begin, unsigned end,
                                 unsigned &k, macro_full_expansion_node &host,
                                 std::vector<macro_full_expansion_node> &pool) {
    for (auto i = begin; i < end; ++i) {
      make_macro_full_expansion_token(parent, k, host, pool);
      auto &node = pool.emplace_back(i, false);
      host.children.emplace_back(&node);
      make_macro_full_expansion(i, i + 1, macros[i].remote + 1, k, node, pool);
      i = macros[i].remote;
    }

    make_macro_full_expansion_token(parent, k, host, pool);
  }

  void make_macro_full_expansion_token(
      int at, unsigned &k, macro_full_expansion_node &host,
      std::vector<macro_full_expansion_node> &pool) {
    unsigned n = expansions.size();
    while (k < n && expansions[k].first == at) {
      host.children.emplace_back(&pool.emplace_back(k, true));
      ++k;
    }
  }

  void dump_full_macro_expansion(const macro_full_expansion_node &node) {
    dumper->AddChild([&, this] {
      if (node.token)
        dump_token(expansions[node.i].second,
                   macros[expansions[node.i].first].info);
      else
        dump_macro_expansion(macros[node.i]);

      for (auto child : node.children) {
        dump_full_macro_expansion(*child);
      }
    });
  }

  void dump_macro_expansion(const macro_expansion_node &macro) {
    out << "MacroExpansion";
    dumper->dumpPointer(macro.info);
    dumper->dumpSourceRange(macro.range);
    if (macro.fast)
      out << " fast";
    out << ' ';
    out << macro.token.getIdentifierInfo()->getName();
  }

  bool is_in_scratch_space(SourceLocation loc) {
    auto &sm = pp.getSourceManager();
    auto fid = sm.getFileID(loc);
    return sm.getBufferOrFake(fid).getBufferIdentifier() == "<scratch space>";
  }

  SourceLocation skip_scratch_space(SourceLocation loc) {
    auto &sm = pp.getSourceManager();
    while (is_in_scratch_space(sm.getSpellingLoc(loc)))
      loc = sm.getImmediateMacroCallerLoc(loc);
    return loc;
  }

  void index(SourceLocation loc, index_value_t value) {
    assert(loc.isFileID());

    FileID fid;
    unsigned offset;
    std::tie(fid, offset) = pp.getSourceManager().getDecomposedLoc(loc);
    auto result = indices[fid].try_emplace(offset, value);
    if (!result.second && result.first->second != value) {
      if (result.first->second.is_decl()) {
        assert(result.first->second.get_decl() ==
               value.get_decl()->getPreviousDecl());
      } else {
        value.get_expanded_decl()->set_previous(
            result.first->second.get_expanded_decl());
      }
      result.first->second = value;
    }
  }

  const index_value_t *find(SourceLocation loc, bool exactly = false) const {
    assert(loc.isFileID());

    FileID fid;
    unsigned offset;
    std::tie(fid, offset) = pp.getSourceManager().getDecomposedLoc(loc);
    if (auto i = indices.find(fid); i != indices.end()) {
      auto j = exactly ? i->second.find(offset) : i->second.lower_bound(offset);
      if (j != i->second.end())
        return &j->second;
    }

    return nullptr;
  }

  void index_macro(unsigned begin, unsigned end) {
    while (begin < end) {
      auto &macro = macros[begin++];
      auto end_loc = macro.info->getDefinitionEndLoc();
      if (end_loc.isValid())
        index(end_loc, index_value_t(macro.info));
      else
        assert(macro.info->isBuiltinMacro());
    }
  }

  const MacroInfo *find_macro(SourceLocation loc) const {
    if (auto v = find(loc); v && v->is_macro()) {
      auto macro = v->get_macro();
      if (SourceRange(macro->getDefinitionLoc(), macro->getDefinitionEndLoc())
              .fullyContains({loc, loc}))
        return macro;
    }

    return nullptr;
  }

  void dump_token(const Token &token, MacroInfo *provider = nullptr) {
    auto &sm = pp.getSourceManager();
    auto raw_loc = token.getLocation();
    auto loc = skip_scratch_space(raw_loc);
    bool is_arg = false;
    const MacroInfo *macro = nullptr;

    if (provider && loc.isMacroID()) {
      auto spelling_loc = sm.getSpellingLoc(loc);
      macro = find_macro(spelling_loc);
      if (!macro) {
        assert(sm.isMacroArgExpansion(loc));
        is_arg = true;
      }
    }

    out << '/';
    out.escape('/');
    dump_token_content(token);
    out.escape(0);
    out << '/';

    SourceLocation origin_loc;
    if (macro && macro != provider) {
      out << " macro " << macro;

      SourceRange range(macro->getDefinitionLoc(),
                        macro->getDefinitionEndLoc());

      auto caller_loc = loc;
      do {
        caller_loc = sm.getImmediateMacroCallerLoc(caller_loc);
        auto spelling_loc = sm.getSpellingLoc(caller_loc);
        if (!range.fullyContains({spelling_loc, spelling_loc})) {
          origin_loc = caller_loc;
          break;
        }
      } while (caller_loc.isMacroID());
    }

    dumper->dumpSourceRange({loc, raw_loc});
    if (origin_loc.isValid()) {
      out << ' ';
      dumper->dumpLocation(origin_loc);
    }

    if (token.hasLeadingSpace())
      out << " hasLeadingSpace";

    if (token.stringifiedInMacro())
      out << " stringified";
    else if (loc != raw_loc)
      out << " paste";

    if (is_arg)
      out << " arg";
  }

  unsigned dump_token_content(const Token &token) {
    if (IdentifierInfo *ii = token.getIdentifierInfo()) {
      auto name = ii->getName();
      out << name;
      return name.size();
    }

    if (token.isLiteral() && !token.needsCleaning() && token.getLiteralData()) {
      out.write(token.getLiteralData(), token.getLength());
      return token.getLength();
    }

    return Lexer::dumpSpelling(token, out, pp.getSourceManager(),
                               pp.getLangOpts());
  }

  raw_line_ostream &out;
  Preprocessor &pp;
  std::unique_ptr<raw_line_ostream> os;
  ast_visitor visitor;
  std::optional<TextNodeDumper> dumper;
  llvm::SmallVector<unsigned, 8> expanding_stack;
  std::vector<macro_expansion_node> macros;
  std::vector<std::pair<unsigned, Token>> expansions;
  std::pair<unsigned, unsigned> last_macro_expansion;
  std::vector<std::variant<directive_def, directive_if, directive_elif,
                           directive_else, directive_endif, directive_ifdef,
                           directive_ifndef, directive_elifdef_taken,
                           directive_elifdef_skiped, directive_elifndef_taken,
                           directive_elifndef_skiped, pseudo_directive_macro>>
      directives;
  unsigned dir; // index of the present directive_node
  std::vector<directive_node> directive_nodes;
  llvm::DenseMap<FileID, std::map<unsigned, index_value_t>> indices;
  std::vector<syntax::Token> tokens;
}; // namespace

std::unique_ptr<ASTConsumer> make_ast_dumper(std::unique_ptr<raw_ostream> os,
                                             CompilerInstance &compiler,
                                             std::string_view in_file) {
  const auto &opts = compiler.getFrontendOpts();
  return CreateASTDumper(std::move(os), opts.ASTDumpFilter, opts.ASTDumpDecls,
                         opts.ASTDumpAll, opts.ASTDumpLookups,
                         opts.ASTDumpDeclTypes, opts.ASTDumpFormat);
}

std::unique_ptr<ASTConsumer>
make_ast_consumer(std::unique_ptr<raw_line_ostream> os,
                  CompilerInstance &compiler, std::string_view in_file) {
  return std::make_unique<ast_consumer>(std::move(os),
                                        compiler.getPreprocessor());
}

class frontend_action : public ASTFrontendAction {
public:
  frontend_action() = default;
  frontend_action(int (*parse_line)(char *line, size_t n, size_t cap,
                                    void *data),
                  void *data)
      : parse_line(parse_line), data(data) {}

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &compiler,
                    llvm::StringRef in_file) override {
    std::vector<std::unique_ptr<ASTConsumer>> v;
    v.push_back(
        make_ast_dumper(std::make_unique<raw_line_ostream>(parse_line, data),
                        compiler, in_file));
    v.push_back(
        make_ast_consumer(std::make_unique<raw_line_ostream>(parse_line, data),
                          compiler, in_file));
    return std::make_unique<MultiplexConsumer>(std::move(v));
  }

private:
  int (*parse_line)(char *line, size_t n, size_t cap, void *data) = nullptr;
  void *data = nullptr;
};

int print_line(char *line, size_t n, size_t cap, void *data) {
  llvm::outs() << std::string_view{line, n};
  return 0;
}

} // namespace

int remark(const char *code, size_t size, const char *filename, char **opts,
           int n,
           int (*parse_line)(char *line, size_t n, size_t cap, void *data),
           void *data) {
  std::vector<std::string> args(opts, opts + n);
  args.push_back("-Xclang");
  args.push_back("-ast-dump");
  args.push_back("-fno-color-diagnostics");

  if (!filename)
    filename = "input.c";
  if (!parse_line)
    parse_line = print_line;

  return clang::tooling::runToolOnCodeWithArgs(
             std::make_unique<frontend_action>(parse_line, data),
             std::string_view{code, size}, args, filename)
             ? 0
             : -1;
}
