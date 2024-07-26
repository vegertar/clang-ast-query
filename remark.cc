#include "remark.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/TextNodeDumper.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Tooling/Syntax/Tokens.h>
#include <clang/Tooling/Tooling.h>

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

struct expansion_decl {
  Token token;
  MacroInfo *info;
  SourceRange range;
  int parent;
  unsigned remote;
  bool fast;
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

  explicit index_value_t(const expansion_decl *p) : data((char *)p + 3) {
    static_assert(alignof(expansion_decl) >= 4 &&
                  alignof(expansion_decl) % 4 == 0);
    assert(is_expansion() && get_raw() == p);
  }

  int kind() const { return ((std::uintptr_t)data & mask); }

  bool is_macro() const { return kind() == 0; }

  bool is_decl() const { return kind() == 1; }

  bool is_expanded_decl() const { return kind() == 2; }

  bool is_expansion() const { return kind() == 3; }

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

  const expansion_decl *get_expansion() const {
    assert(is_expansion());
    return (const expansion_decl *)get_raw();
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
    out << "#TS:" << time(NULL) << '\n';
    out << "#CWD:" << getcwd(cwd, sizeof(cwd)) << '\n';

    for (auto range : inactive_regions) {
      out << "#INACTIVE:";
      dumper->dumpSourceRange(range);
      out << '\n';
    }

    // Dump all root macro expansion ranges to make their locations indexable.
    for (auto &macro : macros) {
      if (macro.parent == -1) {
        SourceLocation loc = sm.getExpansionLoc(macro.token.getLocation());
        SourceLocation end =
            macro.range.getBegin() == macro.range.getEnd()
                ? loc.getLocWithOffset(macro.token.getLength())
                : sm.getExpansionLoc(macro.range.getEnd()).getLocWithOffset(1);
        out << "#LOC-EXP:";
        dumper->dumpSourceRange({loc, end});
        out << '\n';

        // Index the macro to be used by #TOK-DECL in
        // dump_token_expansion().
        index(end, index_value_t(&macro), true);
      }
    }

    visitor.TraverseDecl(ctx.getTranslationUnitDecl());

    if (exported_symbols.size()) {
      out << "#EXPORTED:";
      for (auto p : exported_symbols)
        dumper->dumpPointer(p);
      out << '\n';
    }

    dump_token_expansion();
  }

private:
  class ast_visitor : public RecursiveASTVisitor<ast_visitor> {
  public:
    explicit ast_visitor(ast_consumer &ast) : ast(ast) {}

    bool VisitTypedefDecl(const TypedefDecl *d) {
      auto t = d->getUnderlyingType().getTypePtr();
      if (auto p = named(t); p && !p->isImplicit()) {
        auto loc = get_location(d->getTypeSourceInfo()->getTypeLoc());
        // TagDecl has already been visited
        if (!loc.isMacroID() || !t->getAs<TagType>())
          index_named(p, loc);
      }
      return index_named(d);
    }

    bool VisitFieldDecl(const FieldDecl *d) { return index_valuable(d); }

    bool VisitTagDecl(const TagDecl *d) {
      auto def = d->getDefinition();
      return index_named(def ? def : d, d->getLocation());
    }

    bool VisitVarDecl(const VarDecl *d) {
      const auto kind = d->isThisDeclarationADefinition();
      const auto max_kind = d->hasDefinition();

      if (max_kind == VarDecl::DeclarationOnly) {
        remark_imported_decl(d);
      } else {
        const auto def = max_kind == VarDecl::TentativeDefinition
                             ? d->getActingDefinition()
                             : d->getDefinition();

        if (d != def)
          remark_decl_def(d, def);
        else if (d->isExternC())
          remark_exported_decl(d);
      }

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

      const auto with_body = d->doesThisDeclarationHaveABody();
      const auto index_type_only = !with_body;
      for (auto param : d->parameters())
        index_valuable(param, index_type_only);

      if (!with_body)
        remark_decl_def(d, d->getDefinition());

      if (!d->hasBody())
        remark_imported_decl(d);
      else if (d->isExternC() && with_body && !d->hasAttr<GNUInlineAttr>())
        remark_exported_decl(d);

      return true;
    }

    bool VisitDeclRefExpr(const DeclRefExpr *e) {
      remark_exp_expr(e);
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

    bool VisitInitListExpr(InitListExpr *expr) {
      for (auto init : expr->inits()) {
        if (auto designated = llvm::dyn_cast<DesignatedInitExpr>(init)) {
          for (auto designator : designated->designators()) {
            if (designator.isFieldDesignator()) {
              index_named(designator.getFieldDecl(), designator.getFieldLoc());
            }
          }
        }
      }
      return true;
    }

    bool shouldTraversePostOrder() const { return use_post_order; }

  private:
    // #EXP-EXPR: dump the relevant expressions at the macro expansion point
    void remark_exp_expr(const Expr *e) {
      auto loc = e->getExprLoc();
      if (loc.isMacroID()) {
        auto &sm = ast.pp.getSourceManager();
        ast.out << "#EXP-EXPR:";
        ast.dumper->dumpLocation(sm.getExpansionLoc(loc));
        ast.dumper->dumpPointer(e);
        ast.out << '\n';
      }
    }

    template <typename D> void remark_imported_decl(const D *d) {
      // NOP
    }

    template <typename D> void remark_exported_decl(const D *d) {
      ast.exported_symbols.push_back(d);
    }

    void remark_decl_def(const void *decl, const void *def) {
      ast.out << "#DECL-DEF:";
      ast.dumper->dumpPointer(decl);
      ast.dumper->dumpPointer(def);
      ast.out << '\n';
    }

    void remark_var_type(const void *var, const void *type) {
      ast.out << "#VAR-TYPE:";
      ast.dumper->dumpPointer(var);
      ast.dumper->dumpPointer(type);
      ast.out << '\n';
    }

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
      auto t = p->getType().getTypePtr();

      // Index the type part of the VarDecl. Since the ordinary hover splits
      // the type token from a complete type spec, such as "int" in "int*",
      // hence only the "int" part is indexed.
      if (auto d = named(t, p);
          d && !d->isImplicit() && !d->getDeclName().isEmpty()) {
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

      if (!type_only)
        index_named(p);

      return true;
    }

    NamedDecl *named(const Type *p, const void *var = nullptr,
                     bool *remarked = nullptr) {
      NamedDecl *d = nullptr;
      bool record = false;

      if (auto t = p->getAs<TypedefType>()) {
        auto decl = t->getDecl();
        if (var && decl && (!remarked || !*remarked)) {
          bool done = false;
          if (!remarked)
            remarked = &done;

          // remark var with the underlying type if possible
          named(decl->getUnderlyingType().getTypePtr(), var, remarked);
          if (!*remarked) {
            // otherwise fallback to the type alias that close to the possible
            // underlying type
            remark_var_type(var, decl);
            *remarked = true;
          }
        }

        d = decl;
      } else if (auto t = p->getAs<TagType>()) {
        d = t->getDecl();
        if (var && d) {
          remark_var_type(var, d);
          if (remarked)
            *remarked = true;
        }
      } else if (auto t = p->getAs<FunctionType>()) {
        d = named(t->getReturnType().getTypePtr(), var);
      } else if (auto t = p->getAs<PointerType>()) {
        d = named(t->getPointeeType().getTypePtr());
      } else if (auto t = p->getArrayElementTypeNoTypeQual()) {
        d = named(t);
      }

      return d;
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
      ast.directives.emplace_back(directive_ifdef{loc, token.getLocation(),
                                                  token.getIdentifierInfo(),
                                                  md.getMacroInfo()});
      add_if_directive();
    }

    void Ifndef(SourceLocation loc, const Token &token,
                const MacroDefinition &md) override {
      ast.directives.emplace_back(directive_ifndef{loc, token.getLocation(),
                                                   token.getIdentifierInfo(),
                                                   md.getMacroInfo()});
      add_if_directive();
    }

    void Elifdef(SourceLocation loc, const Token &token,
                 const MacroDefinition &md) override {
      ast.lift_directive();
      ast.directives.emplace_back(directive_elifdef_taken{
          loc, token.getLocation(), token.getIdentifierInfo(),
          md.getMacroInfo()});
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
          loc, token.getLocation(), token.getIdentifierInfo(),
          md.getMacroInfo()});
      add_if_directive();
    }

    void Elifndef(SourceLocation loc, SourceRange range,
                  SourceLocation if_loc) override {
      lift_if_directive(if_loc);
      ast.directives.emplace_back(
          directive_elifndef_skiped{loc, range, if_loc});
      add_if_directive();
    }

    void Defined(const Token &token, const MacroDefinition &md,
                 SourceRange range) override {
      ast.defined_queue.push_back({range.getBegin(), token.getLocation(),
                                   token.getIdentifierInfo(),
                                   md.getMacroInfo()});
    }

    void InclusionDirective(SourceLocation hash_loc, const Token &include_tok,
                            StringRef filename, bool is_angled,
                            CharSourceRange filename_range,
                            OptionalFileEntryRef file, StringRef search_path,
                            StringRef relative_path,
                            const Module *suggested_module,
                            bool module_imported,
                            SrcMgr::CharacteristicKind file_type) override {
      auto include_ii = include_tok.getIdentifierInfo();
      assert(include_ii);

      ast.directives.emplace_back(directive_inclusion{
          hash_loc,
          {filename_range.getBegin(), filename_range.getEnd()},
          include_ii->getName(),
          filename,
          file ? file->getFileEntry().tryGetRealPathName() : "",
          is_angled,
      });

      ast.inclusion_stack.push_back(ast.directives.size() - 1);
      add_sub_directive();
    }

    void FileChanged(SourceLocation loc, FileChangeReason reason,
                     SrcMgr::CharacteristicKind file_type,
                     FileID prev_fid = FileID()) override {
      if (reason == ExitFile && !ast.inclusion_stack.empty()) {
        auto i = ast.inclusion_stack.back();
        auto inclusion = std::get_if<directive_inclusion>(&ast.directives[i]);

        assert(inclusion);
        auto hash_loc = inclusion->loc;

        assert(hash_loc.isFileID() && loc.isFileID());
        auto &sm = ast.pp.getSourceManager();
        auto hash_ploc = sm.getPresumedLoc(hash_loc);
        auto ploc = sm.getPresumedLoc(loc);

        if (hash_ploc.getFileID() == ploc.getFileID() &&
            hash_ploc.getLine() + 1 == ploc.getLine() &&
            ploc.getColumn() == 1) {
          ast.inclusion_stack.pop_back();
          ast.lift_directive();
        }
      }
    }

    void FileSkipped(const FileEntryRef &skipped_file,
                     const Token &filename_tok,
                     SrcMgr::CharacteristicKind file_type) override {
      assert(ast.inclusion_stack.size());
      ast.inclusion_stack.pop_back();
      ast.lift_directive();
    }

    void SourceRangeSkipped(SourceRange range,
                            SourceLocation endif_loc) override {
      assert(endif_loc.isFileID());
      auto &sm = ast.pp.getSourceManager();
      auto ploc = sm.getPresumedLoc(endif_loc);
      auto file = sm.getFileEntryForID(ploc.getFileID());
      assert(file);
      auto path = file->tryGetRealPathName();

      // Check if include recursively
      if (std::find_if(ast.inclusion_stack.rbegin() + 1,
                       ast.inclusion_stack.rend(), [path, this](auto i) {
                         auto inclusion = std::get_if<directive_inclusion>(
                             &ast.directives[i]);
                         assert(inclusion);
                         return inclusion->path == path;
                       }) != ast.inclusion_stack.rend()) {
        return;
      }

      ast.inactive_regions.push_back(range);
    }

    void EndOfMainFile() override { assert(ast.inclusion_stack.empty()); }

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

    void add_if_directive() { add_sub_directive(); }

    void add_sub_directive() {
      ast.add_directive();
      ast.down_directive();
      ast.add_defined_operator();
      ast.add_expansion_directive();
    }

    ast_consumer &ast;
  };

  struct macro_expansion_node {
    unsigned i; // index of macros
    bool token;
    llvm::SmallVector<macro_expansion_node *, 4> children;
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

  struct directive_ifxdef {
    SourceLocation loc;
    SourceLocation tok_loc;
    const IdentifierInfo *id;
    const MacroInfo *mi;
  };

  struct operator_defined : directive_ifxdef {};

  struct directive_ifdef : directive_ifxdef {};

  struct directive_ifndef : directive_ifxdef {};

  struct directive_elifdef_taken : directive_ifxdef {};

  struct directive_elifndef_taken : directive_ifxdef {};

  struct directive_elifdef_skiped {
    SourceLocation loc;
    SourceRange range;
    SourceLocation if_loc;
  };

  struct directive_elifndef_skiped {
    SourceLocation loc;
    SourceRange range;
    SourceLocation if_loc;
  };

  struct directive_expansion {
    macro_expansion_node root;
    std::vector<macro_expansion_node> pool;

    directive_expansion() = default;
    directive_expansion(directive_expansion &&) = default;
    directive_expansion(const directive_expansion &) = delete;
  };

  struct directive_inclusion {
    SourceLocation loc;
    SourceRange range;
    StringRef kind;
    StringRef filename;
    StringRef path;
    bool angled;
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

  void dump_macro_ref(const MacroInfo *mi, StringRef name) {
    if (!mi)
      return;

    out << " '<...>' Macro";
    dumper->dumpPointer(mi);
    out << " '" << name << "' ";
    dump_macro_parameters(mi);
    out << ':';
    dump_macro_replacement(mi);
  }

  void dump_macro_parameters(const MacroInfo *mi) {
    out << "'";
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
    out << "'";
  }

  void dump_macro_replacement(const MacroInfo *mi) {
    out << "'";
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

  void dump_macro(const MacroInfo *mi, StringRef name) {
    out << "MacroDecl";
    dumper->dumpPointer(mi);
    dumper->dumpSourceRange(
        {mi->getDefinitionLoc(), mi->getDefinitionEndLoc()});

    out << ' ' << name << ' ';
    dump_macro_parameters(mi);
    out << ':';
    dump_macro_replacement(mi);
  }

  void on_token_lexed(const Token &token) {
    // Collect all tokens to dump in later.
    if (!token.isAnnotation())
      tokens.push_back(syntax::Token(token));

    auto loc = token.getLocation();
    if (loc.isFileID()) {
      add_expansion_directive();
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
              auto id = arg.id;
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
              dumper->AddChild([id, md, this] {
                dump_macro(md->getMacroInfo(), id->getName());
              });
            } else if constexpr (std::is_same_v<T, directive_if> ||
                                 std::is_same_v<T, directive_elif>) {
              if constexpr (std::is_same_v<T, directive_if>)
                out << "IfDirective";
              else if constexpr (std::is_same_v<T, directive_elif>)
                out << "ElifDirective";
              else
                assert("Never reach here");

              dumper->dumpPointer(&arg);
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
              out << "ElseDirective";
              dumper->dumpPointer(&arg);
              dumper->dumpSourceRange({arg.loc, arg.if_loc});
            } else if constexpr (std::is_same_v<T, directive_endif>) {
              out << "EndifDirective";
              dumper->dumpPointer(&arg);
              dumper->dumpSourceRange({arg.loc, arg.if_loc});
            } else if constexpr (std::derived_from<T, directive_ifxdef>) {
              if constexpr (std::is_same_v<T, operator_defined>)
                out << "DefinedOperator";
              else if constexpr (std::is_same_v<T, directive_ifdef>)
                out << "IfdefDirective";
              else if constexpr (std::is_same_v<T, directive_elifdef_taken>)
                out << "ElifdefDirective";
              else if constexpr (std::is_same_v<T, directive_ifndef>)
                out << "IfndefDirective";
              else if constexpr (std::is_same_v<T, directive_elifndef_taken>)
                out << "ElifndefDirective";
              else
                assert("Never reach here");

              dumper->dumpPointer(&arg);
              // Simulate the SourceRange of the expression being tested in
              // #if/elif callback
              dumper->dumpSourceRange(
                  {arg.tok_loc,
                   arg.tok_loc.getLocWithOffset(arg.id->getLength())});
              out << ' ';
              dumper->dumpLocation(arg.loc);
              if (arg.mi)
                dump_macro_ref(arg.mi, arg.id->getName());
              else
                out << ' ' << arg.id->getName();
            } else if constexpr (std::is_same_v<T, directive_elifdef_skiped> ||
                                 std::is_same_v<T, directive_elifndef_skiped>) {
              if constexpr (std::is_same_v<T, directive_elifdef_skiped>)
                out << "ElifdefDirective";
              else if constexpr (std::is_same_v<T, directive_elifndef_skiped>)
                out << "ElifndefDirective";
              else
                assert("Never reach here");

              dumper->dumpPointer(&arg);
              dumper->dumpSourceRange(arg.range);
              out << ' ';
              dumper->dumpLocation(arg.loc);
            } else if constexpr (std::is_same_v<T, directive_expansion>) {
              out << "ExpansionDirective";
              dumper->dumpPointer(&arg);
              for (auto child : arg.root.children) {
                dump_macro_expansion(*child);
              }
            } else if constexpr (std::is_same_v<T, directive_inclusion>) {
              out << "InclusionDirective";
              dumper->dumpPointer(&arg);
              dumper->dumpSourceRange(arg.range);
              out << ' ';
              dumper->dumpLocation(arg.loc);
              if (arg.angled)
                out << " angled";
              out << ' ' << arg.kind << " '" << arg.filename << "'";
              if (arg.path.size())
                out << ":'" << arg.path << "'";
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
    int index_of_last_expansion_loc = -1;
    unsigned indexable_tokens = 0;
    unsigned indexed_tokens = 0;

    for (unsigned i = 0, e = tokens.size(); i < e; ++i) {
      auto loc = tokens[i].location();
      if (indexable(tokens[i])) {
        ++indexable_tokens;

        if (loc.isFileID()) {
          auto v = find(loc, FIND_OPTION_EQUAL);

          out << "#TOK-DECL:";
          dumper->dumpLocation(tokens[i].location());

          auto d = v ? v->get_decl() : nullptr;
          if (d)
            ++indexed_tokens;
          dumper->dumpPointer(d);
          out << '\n';
        }
      }

      auto expansion_loc = sm.getExpansionLoc(loc);
      if (expansion_loc != last_expansion_loc) {
        if (last_loc.isMacroID()) {
          auto v = find(last_expansion_loc, FIND_OPTION_EQUAL);
          auto d = v ? v->get_expanded_decl() : nullptr;
          int j = i - 1;

          while (d && j > index_of_last_expansion_loc) {
            auto &t = tokens[j];
            if (indexable(t)) {
              out << "#TOK-DECL:";
              dumper->dumpLocation(t.location());
              out << ' ' << j - index_of_last_expansion_loc;

              const Decl *p = nullptr;
              if (t.location() == d->get_location()) {
                ++indexed_tokens;
                p = d->get_decl();
                auto prev = d->get_previous();
                delete d;
                d = prev;
              }
              dumper->dumpPointer(p);
              out << '\n';
            }
            --j;
          }

          // Ending with an explicit zero-offset and the macro.
          out << "#TOK-DECL:";
          dumper->dumpLocation(last_expansion_loc);
          out << " 0";
          // MACRO_FOO(a, b, c, e, f, g);
          // ^~~expanded_decl           ^~~expansion_decl
          v = find(last_expansion_loc, FIND_OPTION_UPPER_BOUND);
          assert(v && v->is_expansion());
          dumper->dumpPointer(v->get_expansion());
          out << '\n';

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

  void down_directive() { dir = directive_nodes[dir].children.back(); }

  void add_directive() {
    directive_nodes.emplace_back(directives.size() - 1, dir);
    directive_nodes[dir].children.emplace_back(directive_nodes.size() - 1);
  }

  void add_defined_operator() {
    for (auto &item : defined_queue) {
      directives.push_back(item);
      add_directive();
    }
    defined_queue.clear();
  }

  void add_expansion_directive() {
    assert(expanding_stack.empty());

    unsigned k = last_macro_expansion.second;
    directive_expansion exp;
    exp.pool.reserve(macros.size() - last_macro_expansion.first +
                     expansions.size() - last_macro_expansion.second);

    index_macro(last_macro_expansion.first, macros.size());
    make_macro_expansion(-1, last_macro_expansion.first, macros.size(), k,
                         exp.root, exp.pool);
    assert(exp.pool.size() == macros.size() - last_macro_expansion.first +
                                  expansions.size() -
                                  last_macro_expansion.second);

    if (!exp.pool.empty()) {
      directives.emplace_back(std::move(exp));
      last_macro_expansion.first = macros.size();
      last_macro_expansion.second = expansions.size();
      add_directive();
    }
  }

  void make_macro_expansion(int parent, unsigned begin, unsigned end,
                            unsigned &k, macro_expansion_node &host,
                            std::vector<macro_expansion_node> &pool) {
    for (auto i = begin; i < end; ++i) {
      make_macro_expansion_token(parent, k, host, pool);
      auto &node = pool.emplace_back(i, false);
      host.children.emplace_back(&node);
      make_macro_expansion(i, i + 1, macros[i].remote + 1, k, node, pool);
      i = macros[i].remote;
    }

    make_macro_expansion_token(parent, k, host, pool);
  }

  void make_macro_expansion_token(int at, unsigned &k,
                                  macro_expansion_node &host,
                                  std::vector<macro_expansion_node> &pool) {
    unsigned n = expansions.size();
    while (k < n && expansions[k].first == at) {
      host.children.emplace_back(&pool.emplace_back(k, true));
      ++k;
    }
  }

  void dump_macro_expansion(const macro_expansion_node &node) {
    dumper->AddChild([&, this] {
      if (node.token)
        dump_token(expansions[node.i].second,
                   macros[expansions[node.i].first].info);
      else
        dump_macro_expansion(macros[node.i]);

      for (auto child : node.children) {
        dump_macro_expansion(*child);
      }
    });
  }

  void dump_macro_expansion(const expansion_decl &macro) {
    out << "ExpansionDecl";
    dumper->dumpPointer(&macro);
    dumper->dumpSourceRange(macro.range);
    if (macro.fast)
      out << " fast";
    dump_macro_ref(macro.info, macro.token.getIdentifierInfo()->getName());
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

  void index(SourceLocation loc, index_value_t value,
             bool ensure_newly = false) {
    assert(loc.isFileID());

    FileID fid;
    unsigned offset;
    std::tie(fid, offset) = pp.getSourceManager().getDecomposedLoc(loc);
    auto result = indices[fid].try_emplace(offset, value);
    if (!result.second && result.first->second != value) {
      assert(!ensure_newly && "Not allowed to insert by the same key.");
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

  enum find_option {
    FIND_OPTION_LOWER_BOUND,
    FIND_OPTION_EQUAL,
    FIND_OPTION_UPPER_BOUND,
  };

  const index_value_t *find(SourceLocation loc,
                            find_option opt = FIND_OPTION_LOWER_BOUND) const {
    assert(loc.isFileID());

    FileID fid;
    unsigned offset;
    std::tie(fid, offset) = pp.getSourceManager().getDecomposedLoc(loc);
    if (auto i = indices.find(fid); i != indices.end()) {
      auto j = opt == FIND_OPTION_EQUAL
                   ? i->second.find(offset)
                   : opt == FIND_OPTION_LOWER_BOUND
                         ? i->second.lower_bound(offset)
                         : opt == FIND_OPTION_UPPER_BOUND
                               ? i->second.upper_bound(offset)
                               : i->second.end();
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
  llvm::SmallVector<unsigned, 8> inclusion_stack;
  llvm::SmallVector<operator_defined, 8> defined_queue;
  std::vector<expansion_decl> macros;
  std::vector<std::pair<unsigned, Token>> expansions;
  std::pair<unsigned, unsigned> last_macro_expansion;
  std::vector<std::variant<directive_def, directive_if, directive_elif,
                           directive_else, directive_endif, operator_defined,
                           directive_ifdef, directive_ifndef,
                           directive_elifdef_taken, directive_elifdef_skiped,
                           directive_elifndef_taken, directive_elifndef_skiped,
                           directive_expansion, directive_inclusion>>
      directives;
  unsigned dir; // index of the present directive_node
  std::vector<directive_node> directive_nodes;
  llvm::DenseMap<FileID, std::map<unsigned, index_value_t>> indices;
  std::vector<syntax::Token> tokens;
  std::vector<SourceRange> inactive_regions;
  std::vector<const void *> exported_symbols;
};

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
