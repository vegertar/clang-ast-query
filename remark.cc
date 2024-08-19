#include "remark.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/TextNodeDumper.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Tooling/Syntax/Tokens.h>
#include <clang/Tooling/Tooling.h>

#include <cctype>
#include <optional>
#include <type_traits>
#include <unistd.h>

namespace {

using namespace clang;

static inline const char *get_macro_kind(const MacroInfo *info) {
  if (!info)
    return "identifier";

  unsigned kind = 0;
  if (info->isBuiltinMacro())
    kind |= 1;
  if (info->isFunctionLike())
    kind |= 2;

  switch (kind) {
  case 1:
    return "built_in_macro";
  case 2:
    return "function_like_macro";
  case 3:
    return "built_in_function_like_macro";
  default:
    return "macro";
  }
}

static inline SourceLocation get_macro_end(const MacroInfo *info,
                                           const IdentifierInfo *id) {
  if (auto body = info->tokens(); !body.empty()) {
    auto &last_token = body.back();
    return last_token.getLocation().getLocWithOffset(last_token.getLength());
  }

  return info->getDefinitionEndLoc().getLocWithOffset(
      info->isFunctionLike() ? 1 : id->getName().size());
}

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

struct macro_test {
  IdentifierInfo *ii;
  MacroInfo *mi;
  SourceRange sr;

  macro_test(const Token &token, const MacroDefinition &md)
      : ii(token.getIdentifierInfo()), mi(md.getMacroInfo()),
        sr(token.getLocation(),
           token.getLocation().getLocWithOffset(token.getLength())) {}
};

struct macro_expansion {
  Token token; // TODO: use IdentifierInfo instead
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

  explicit index_value_t(const macro_expansion *p) : data((char *)p + 3) {
    static_assert(alignof(macro_expansion) >= 4 &&
                  alignof(macro_expansion) % 4 == 0);
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

  const macro_expansion *get_expansion() const {
    assert(is_expansion());
    return (const macro_expansion *)get_raw();
  }

  void *get_raw() const { return (void *)((std::uintptr_t)data & ~mask); }

  bool operator==(index_value_t other) const {
    return get_raw() == other.get_raw();
  }

private:
  void *data;
  static const std::uintptr_t mask = 3;
};

struct semantic_token {
  SourceRange range;
  int kind;
  bool is_pp;

  static constexpr std::pair<const char *, const char *> token_names[] = {
#define TOK(X) {"TOKEN", #X},
#define KEYWORD(X, Y) {"KEYWORD", #X},
#define PUNCTUATOR(X, Y) {"PUNCTUATION", #X},
#include "clang/Basic/TokenKinds.def"
      {nullptr, nullptr}};

  static std::pair<const char *, const char *> token_name(int kind) {
    std::pair<const char *, const char *> v = semantic_token::token_names[kind];

    if (tok::isLiteral(static_cast<tok::TokenKind>(kind)))
      v.first = "LITERAL";
    else if (kind == tok::identifier)
      v.first = "IDENTIFIER";

    return v;
  }

  std::pair<const char *, const char *> token_name() {
    std::pair<const char *, const char *> v = {};
    if (!is_pp) {
      v = semantic_token::token_name(kind);
    } else {
      v.first = "PPKEYWORD";
      v.second =
          tok::getPPKeywordSpelling(static_cast<tok::PPKeywordKind>(kind));
    }
    return v;
  }
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
        last_expansion(0, 0), dir(0) {
    directive_nodes.emplace_back();
  }

protected:
  void Initialize(ASTContext &context) override {
    dumper.emplace(out, context, false);
    pp.addPPCallbacks(std::make_unique<pp_callback>(*this));
    pp.setTokenWatcher([this](auto &token) { on_token_lexed(token); });
  }

  void HandleTranslationUnit(ASTContext &ctx) override {
    // Some macros are built-in and do not trigger callbacks,
    // so we explicitly define them.
    for (auto &macro : tests) {
      if (macro.mi) {
        if (auto iter = defined_map.find(macro.mi); iter == defined_map.end()) {
          assert(macro.mi->isBuiltinMacro());
          auto md = pp.getLocalMacroDirective(macro.ii);
          assert(md->getMacroInfo() == macro.mi);
          directives.emplace_back(def_directive{macro.ii, md});
          add_directive();
        }
      }
    }

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

    for (auto &token : semantic_tokens) {
      std::pair<const char *, const char *> name = token.token_name();

      out << "#TOK-KIND:";
      dumper->dumpSourceRange(token.range);
      out << " \"" << name.first << ' ' << name.second << "\"\n";
    }

    for (auto &macro : tests) {
      out << "#TOK-KIND:";
      dumper->dumpSourceRange(macro.sr);
      out << " \"IDENTIFIER " << get_macro_kind(macro.mi) << "\"\n";

      if (macro.mi) {
        out << "#TOK-DECL:";
        dumper->dumpLocation(macro.sr.getBegin());
        out << " -2"; // use offset of -2 for testing only macros
        dumper->dumpPointer(macro.mi);
        out << '\n';
      }
    }

    // Dump all root macro expansion ranges to make their locations indexable.
    for (auto &macro : macros) {
      if (macro.parent == -1) {
        SourceLocation loc = sm.getExpansionLoc(macro.token.getLocation());
        SourceLocation tok_end = loc.getLocWithOffset(macro.token.getLength());
        SourceLocation end =
            macro.range.getBegin() == macro.range.getEnd()
                ? tok_end
                : sm.getExpansionLoc(macro.range.getEnd()).getLocWithOffset(1);

        out << "#TOK-KIND:";
        dumper->dumpSourceRange({loc, tok_end});
        out << " \"IDENTIFIER " << get_macro_kind(macro.info) << "\"\n";

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

    if (!ctx.Comments.empty()) {
      static const char *comment_names[] = {
          "invalid",           "bcpl_comment",
          "c_comment",         "bcpl_slash_comment",
          "bcpl_excl_comment", "java_doc_comment",
          "qt_comment",        "merged_comment",
      };

      for (auto begin = sm.fileinfo_begin(), end = sm.fileinfo_end();
           begin != end; ++begin) {
        auto fid = sm.translateFile(begin->first);
        if (auto comments = ctx.Comments.getCommentsInFile(fid)) {
          for (auto &item : *comments) {
            auto raw_comment = item.second;
            assert(raw_comment);
            out << "#TOK-KIND:";
            dumper->dumpSourceRange(raw_comment->getSourceRange());

            assert(static_cast<int>(raw_comment->getKind()) <
                   sizeof(comment_names) / sizeof(*comment_names));
            out << " \"COMMENT " << comment_names[raw_comment->getKind()]
                << "\"\n";
          }
        }
      }
    }

    if (!trivial_expansions.empty()) {
      for (auto i : trivial_expansions) {
        auto &macro = macros[i];
        out << "#TOK-DECL:";
        dumper->dumpLocation(macro.token.getLocation());
        out << " 0";
        dumper->dumpPointer(&macro);
        out << '\n';
      }
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
      ast.defined_map[md->getMacroInfo()] = token.getIdentifierInfo();
      ast.directives.emplace_back(def_directive{token.getIdentifierInfo(), md});
      ast.add_directive();
    }

    void MacroExpands(const Token &token, const MacroDefinition &def,
                      SourceRange range, const MacroArgs *args) override {
      auto &expanding_stack = ast.expanding_stack;
      auto &macros = ast.macros;
      auto remote = macros.size();
      int parent = expanding_stack.empty() ? -1 : expanding_stack.back();
      assert(parent == -1 ? token.getLocation() == range.getBegin() : true);
      macros.emplace_back(
          token, def.getMacroInfo(),
          SourceRange{range.getBegin(),
                      parent != -1 ? range.getEnd()
                                   : range.getBegin() == range.getEnd()
                                         ? token.getLocation().getLocWithOffset(
                                               token.getLength())
                                         : range.getEnd().getLocWithOffset(1)},
          parent, remote);
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
      handle_if(loc, range, value);
    }

    void Elif(SourceLocation loc, SourceRange range, ConditionValueKind value,
              SourceLocation if_loc) override {
      assert(verify_if_loc(if_loc));
      handle_if(loc, range, value, true);
    }

    void Else(SourceLocation loc, SourceLocation if_loc) override {
      assert(verify_if_loc(if_loc));
      auto locations = ast.find_if_locations(loc);
      add_sup_directive<if_directive::block>(
          locations.hash_loc, SourceRange{locations.keyword_end});
    }

    void Endif(SourceLocation loc, SourceLocation if_loc) override {
      auto locations = ast.find_if_locations(loc);
      auto body_end_loc = locations.hash_loc;

      // Either #else or #elif>if_directive::block
      directive_node *current_node = &ast.directive_nodes[ast.dir];
      if_directive *p = nullptr;

      do {
        auto parent_node = &ast.directive_nodes[current_node->parent];
        p = std::get_if<if_directive>(&ast.directives[parent_node->i]);
        assert(p);
        p->range.setEnd(locations.keyword_end);
        p->has_else = parent_node->children.size() == 3;

        auto cond_node = &ast.directive_nodes[parent_node->children[0]];
        auto cond_expr = &ast.directives[cond_node->i];
        auto cond = std::get_if<if_directive::cond>(cond_expr);
        assert(cond);
        // The original end is the position of the last token
        cond->range.setEnd(ast.next_space(cond->range.getEnd()));

        auto then_node = &ast.directive_nodes[parent_node->children[1]];
        auto then_stmt = &ast.directives[then_node->i];
        auto then = std::get_if<if_directive::block>(then_stmt);
        assert(then);
        then->range.setBegin(cond->range.getEnd());
        if (!p->has_else) {
          then->range.setEnd(body_end_loc);
        } else {
          auto alt_node = &ast.directive_nodes[parent_node->children[2]];
          auto directive = &ast.directives[alt_node->i];
          if (auto elif = std::get_if<if_directive>(directive)) {
            then->range.setEnd(elif->range.getBegin());
          } else if (auto alt = std::get_if<if_directive::block>(directive)) {
            assert(alt->hash_loc.isValid()); // is an #else directive
            then->range.setEnd(alt->hash_loc);
            alt->range.setEnd(body_end_loc);
          } else {
            assert(false && "Never reach here");
          }
        }

        ast.dir = current_node->parent;
        current_node = parent_node;
      } while (!p->is_if());

      assert(p->loc == if_loc);
      ast.dir = current_node->parent;
    }

    void Ifdef(SourceLocation loc, const Token &token,
               const MacroDefinition &md) override {
      handle_ifdef(loc, token, md, tok::pp_ifdef);
    }

    void Ifndef(SourceLocation loc, const Token &token,
                const MacroDefinition &md) override {
      handle_ifdef(loc, token, md, tok::pp_ifndef);
    }

    void Elifdef(SourceLocation loc, const Token &token,
                 const MacroDefinition &md) override {
      handle_ifdef(loc, token, md, tok::pp_elifdef);
    }

    void Elifdef(SourceLocation loc, SourceRange range,
                 SourceLocation if_loc) override {
      assert(verify_if_loc(if_loc));
      handle_elifdef(loc, range);
    }

    void Elifndef(SourceLocation loc, const Token &token,
                  const MacroDefinition &md) override {
      handle_ifdef(loc, token, md, tok::pp_elifndef);
    }

    void Elifndef(SourceLocation loc, SourceRange range,
                  SourceLocation if_loc) override {
      assert(verify_if_loc(if_loc));
      handle_elifdef(loc, range, true);
    }

    void Defined(const Token &token, const MacroDefinition &md,
                 SourceRange range) override {
      ast.tests.emplace_back(token, md);
      ast.defined_queue.emplace_back(range, token.getLocation(),
                                     token.getIdentifierInfo(),
                                     md.getMacroInfo());
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
          include_tok.getLocation(),
          {filename_range.getBegin(), filename_range.getEnd()},
          include_ii->getName(),
          filename,
          file ? file->getFileEntry().tryGetRealPathName() : "",
          is_angled,
      });

      ast.inclusion_stack.push_back(ast.directives.size() - 1);
      // add_sub_directive();
    }

    void FileChanged(SourceLocation loc, FileChangeReason reason,
                     SrcMgr::CharacteristicKind file_type,
                     FileID prev_fid = FileID()) override {
      if (reason == ExitFile && !ast.inclusion_stack.empty()) {
        auto i = ast.inclusion_stack.back();
        auto inclusion = std::get_if<directive_inclusion>(&ast.directives[i]);

        assert(inclusion);
        auto hash_loc = inclusion->hash_loc;

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
    void handle_if(SourceLocation loc, SourceRange range,
                   ConditionValueKind value, bool elif = false) {
      auto locations = ast.find_if_locations(loc);

      if (elif)
        ast.lift_directive();

      add_sub_directive<if_directive>(elif ? tok::pp_elif : tok::pp_if, loc,
                                      SourceRange{locations.hash_loc});
      add_sub_directive<if_directive::cond>(range, value);
      ast.add_expansion();
      add_sup_directive<if_directive::block>();
    }

    void handle_ifdef(SourceLocation loc, const Token &token,
                      const MacroDefinition &md, tok::PPKeywordKind kind) {
      auto locations = ast.find_if_locations(loc);
      auto tok_loc = token.getLocation();
      SourceRange range(tok_loc, tok_loc.getLocWithOffset(token.getLength()));
      auto mi = md.getMacroInfo();
      bool is_ndef = kind == tok::pp_ifndef || kind == tok::pp_elifndef;

      if (kind == tok::pp_elifdef || kind == tok::pp_elifndef)
        ast.lift_directive();

      add_sub_directive<if_directive>(kind, loc,
                                      SourceRange{locations.hash_loc});
      add_sub_directive<if_directive::cond>(
          range,
          !!mi == !is_ndef ? pp_callback::CVK_True : pp_callback::CVK_False,
          true);
      ast.add_directive<if_directive::defined>(range, tok_loc,
                                               token.getIdentifierInfo(), mi);
      add_sup_directive<if_directive::block>();
    }

    void handle_elifdef(SourceLocation loc, SourceRange range,
                        bool ndef = false) {
      auto locations = ast.find_if_locations(loc);

      add_sup_directive<if_directive>(ndef ? tok::pp_elifndef : tok::pp_elifdef,
                                      loc, SourceRange{locations.hash_loc});
      add_sub_directive<if_directive::cond>(
          range, pp_callback::CVK_NotEvaluated, true);
      add_sup_directive<if_directive::block>();
    }

    template <typename T, typename... U> void add_sub_directive(U &&... args) {
      ast.add_directive<T>(std::forward<U>(args)...);
      ast.down_directive();
    }

    template <typename T, typename... U> void add_sup_directive(U &&... args) {
      ast.lift_directive();
      add_sub_directive<T>(std::forward<U>(args)...);
    }

    bool verify_if_loc(SourceLocation if_loc) {
      // Must be if>if_directive::block
      directive_node *current_node = &ast.directive_nodes[ast.dir];
      assert(std::holds_alternative<if_directive::block>(
          ast.directives[current_node->i]));

      if_directive *p = nullptr;

      do {
        auto parent_node = &ast.directive_nodes[current_node->parent];
        p = std::get_if<if_directive>(&ast.directives[parent_node->i]);
        assert(p);
        current_node = parent_node;
      } while (!p->is_if());

      return p->loc == if_loc;
    }

    ast_consumer &ast;
  };

  struct expansion {
    struct node {
      unsigned i; // index of macros or macro_tokens if the token field is true
      bool token;
      llvm::SmallVector<node *, 4> children;
    };

    node root; // only root.children is available
    std::vector<node> pool;

    expansion() = default;
    expansion(expansion &&) = default;
    expansion(const expansion &) = delete;
  };

  struct def_directive {
    const IdentifierInfo *id;
    const MacroDirective *md;
  };

  struct if_directive {
    tok::PPKeywordKind kind;
    SourceLocation loc;
    SourceRange range;
    bool has_else;

    bool is_if() const {
      return kind == tok::pp_if || kind == tok::pp_ifdef ||
             kind == tok::pp_ifndef;
    }

    struct cond {
      SourceRange range;
      pp_callback::ConditionValueKind value;
      bool implicit;
    };

    struct defined {
      SourceRange range;
      SourceLocation tok_loc;
      const IdentifierInfo *id;
      const MacroInfo *mi;
    };

    struct block {
      SourceLocation hash_loc;
      SourceRange range;
    };
  };

  struct directive_inclusion {
    SourceLocation hash_loc;
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

  struct directive_locations {
    SourceLocation hash_loc;
    SourceLocation keyword_loc;
    SourceLocation keyword_end;
  };

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

      // Don't dump user defined macro content completely
      auto n =
          (pp.getSourceManager().isWrittenInBuiltinFile(mi->getDefinitionLoc()))
              ? std::numeric_limits<unsigned>::max()
              : 3U;

      while (i != e && n > 0) {
        if (i->hasLeadingSpace())
          out << ' ';
        dump_token_content(*i++);
        --n;
      }

      if (i != e)
        out << "...";
    }
    out.escape(0);
    out << "'";
  }

  void dump_macro(const MacroInfo *mi, const IdentifierInfo *id) {
    out << "MacroPPDecl";
    dumper->dumpPointer(mi);
    dumper->dumpSourceRange({mi->getDefinitionLoc(), get_macro_end(mi, id)});

    out << ' ';
    dump_name(id);
    out << ' ';
    dump_macro_parameters(mi);
    out << ' ';
    dump_macro_replacement(mi);
  }

  void on_token_lexed(const Token &token) {
    // Collect all tokens to dump in later.
    if (!token.isAnnotation())
      tokens.push_back(token);

    auto loc = token.getLocation();
    if (loc.isFileID()) {
      add_expansion();
    } else {
      // The fast-expanded token is expanded at the parent macro, in this case
      // the expanding_stack will be empty in advance.
      macro_tokens.emplace_back(
          expanding_stack.empty() ? macros.size() - 1 : expanding_stack.back(),
          token);
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

  char get(SourceLocation loc, bool *invalid = nullptr) {
    auto s = Lexer::getSourceText(
        CharSourceRange::getCharRange({loc, loc.getLocWithOffset(1)}),
        pp.getSourceManager(), pp.getLangOpts(), invalid);
    return s.empty() ? 0 : s[0];
  }

  StringRef get(SourceLocation loc, SourceLocation end) {
    return Lexer::getSourceText(CharSourceRange::getCharRange({loc, end}),
                                pp.getSourceManager(), pp.getLangOpts());
  }

  SourceLocation prev(SourceLocation loc, bool space, bool *invalid = nullptr) {
    bool err = false;
    int curr = 0;
    SourceLocation last = loc;
    while ((curr = get(loc, &err)) && !err) {
      if (curr == '\n') {
        int prev = get(loc.getLocWithOffset(-1), &err);
        if (!err && prev == '\\') {
          loc = loc.getLocWithOffset(-2);
          continue;
        }
      }

      if (space == static_cast<bool>(std::isspace(curr)))
        break;

      last = loc;
      loc = loc.getLocWithOffset(-1);
    }
    if (invalid)
      *invalid = err;
    return err ? last : loc;
  }

  SourceLocation prev_space(SourceLocation loc, bool *invalid = nullptr) {
    return prev(loc, true, invalid);
  }

  SourceLocation prev_non_space(SourceLocation loc, bool *invalid = nullptr) {
    return prev(loc, false, invalid);
  }

  SourceLocation next(SourceLocation loc, bool space, bool *invalid = nullptr) {
    bool err = false;
    int curr = 0;
    while ((curr = get(loc, &err)) && !err) {
      if (curr == '\\') {
        int next = get(loc.getLocWithOffset(1), &err);
        if (!err && next == '\n') {
          loc = loc.getLocWithOffset(2);
          continue;
        }
      }

      if (space == static_cast<bool>(std::isspace(curr)))
        break;

      loc = loc.getLocWithOffset(1);
    }
    if (invalid)
      *invalid = err;
    return loc;
  }

  SourceLocation next_space(SourceLocation loc, bool *invalid = nullptr) {
    return next(loc, true, invalid);
  }

  SourceLocation next_non_space(SourceLocation loc, bool *invalid = nullptr) {
    return next(loc, false, invalid);
  }

  directive_locations find_define_locations(SourceLocation loc) {
    if (loc.isInvalid())
      return {};

    bool err = false;
    auto keyword_last = prev_non_space(loc.getLocWithOffset(-1), &err);
    assert(!err);

    auto keyword_rend = prev_space(keyword_last.getLocWithOffset(-1), &err);
    // If there's an error, keyword_rend is the file start.
    auto keyword_loc = keyword_rend.getLocWithOffset(err ? 0 : 1);
    auto keyword_end = keyword_last.getLocWithOffset(1);
    auto text = get(keyword_loc, keyword_end);

    SourceLocation hash_loc;
    if (text[0] == '#') {
      hash_loc = keyword_loc;
      keyword_loc = keyword_loc.getLocWithOffset(1);
    } else {
      assert(text[0] == 'd');
      hash_loc = prev_non_space(keyword_rend.getLocWithOffset(-1));
    }

    return {hash_loc, keyword_loc, keyword_end};
  }

  directive_locations find_if_locations(SourceLocation loc) {
    SourceLocation hash_loc = loc.getLocWithOffset(-1);
    auto prev = get(hash_loc);

    if (std::isspace(prev))
      hash_loc = prev_non_space(hash_loc.getLocWithOffset(-1));
    else
      assert(prev == '#');

    /* if/elif/else/endif */
    SourceLocation keyword_end = next_space(loc.getLocWithOffset(1));

    return {hash_loc, loc, keyword_end};
  }

  // void add_pp_defined(SourceLocation loc) {
  //   semantic_tokens.emplace_back(
  //       SourceRange{loc, loc.getLocWithOffset(7) /* defined */},
  //       tok::pp_defined, true);
  // }

  // void add_pp_if(SourceLocation loc, tok::PPKeywordKind kind) {
  //   auto &sm = pp.getSourceManager();
  //   SourceLocation hash_loc = loc.getLocWithOffset(-1);
  //   auto prev = get(hash_loc);

  //   if (std::isspace(prev))
  //     hash_loc = rfind(hash_loc.getLocWithOffset(-1), not_space);
  //   else
  //     assert(prev == '#');

  //   semantic_tokens.emplace_back(
  //       SourceRange{hash_loc, hash_loc.getLocWithOffset(1)}, tok::hash);
  //   semantic_tokens.emplace_back(
  //       SourceRange{loc, find(loc.getLocWithOffset(2) /* if/elif/else/endif
  //       */,
  //                             std::isspace)},
  //       kind, true);
  // }

  // void add_pp_include(SourceLocation hash_loc, SourceLocation loc) {
  //   semantic_tokens.emplace_back(
  //       SourceRange{hash_loc, hash_loc.getLocWithOffset(1)}, tok::hash);
  //   semantic_tokens.emplace_back(
  //       SourceRange{loc, loc.getLocWithOffset(7) /* include */},
  //       tok::pp_include, true);
  // }

  void add_tok(SourceRange range, int kind) {
    semantic_tokens.emplace_back(range, kind);
  }

  bool is_written_internally(SourceLocation loc) {
    auto &sm = pp.getSourceManager();
    return sm.isWrittenInBuiltinFile(loc) ||
           sm.isWrittenInCommandLineFile(loc) ||
           sm.isWrittenInScratchSpace(loc);
  }

  struct directive_dumper {
    ast_consumer &ast;

    void operator()(const def_directive &arg) {
      auto &ast = this->ast;
      auto id = arg.id;
      auto md = arg.md;
      auto mi = md->getMacroInfo();
      auto loc = md->getLocation();

      switch (md->getKind()) {
      case MacroDirective::MD_Define:
        ast.out << "DefDirective";
        break;
      case MacroDirective::MD_Undefine:
        ast.out << "UndefDirective";
        break;
      case MacroDirective::MD_Visibility:
        ast.out << "VisibilityDirective";
        break;
      }

      ast.dumper->dumpPointer(md);
      if (auto prev = md->getPrevious()) {
        ast.out << " prev";
        ast.dumper->dumpPointer(prev);
      }

      SourceRange range;
      if (auto locations = ast.find_define_locations(loc);
          locations.hash_loc.isValid()) {
        range.setBegin(locations.hash_loc);
        range.setEnd(get_macro_end(mi, id));

        if (!ast.is_written_internally(locations.hash_loc)) {
          ast.semantic_tokens.emplace_back(
              SourceRange{locations.hash_loc,
                          locations.hash_loc.getLocWithOffset(1)},
              tok::hash);
          ast.semantic_tokens.emplace_back(
              SourceRange{locations.keyword_loc, locations.keyword_end},
              tok::pp_define, true);
        }
      }

      ast.dumper->dumpSourceRange(range);
      ast.out << ' ';
      ast.dumper->dumpLocation(loc);
      ast.dumper->AddChild([&ast, id, mi] { ast.dump_macro(mi, id); });
    }

    void operator()(const if_directive &arg) {
      ast.out << "IfDirective";
      ast.dumper->dumpPointer(&arg);
      ast.dumper->dumpSourceRange(arg.range);

      ast.out << ' ' << tok::getPPKeywordSpelling(arg.kind);
      if (arg.has_else)
        ast.out << " has_else";
    }

    void operator()(const if_directive::cond &arg) {
      ast.out << "ConditionalPPExpr";
      ast.dumper->dumpPointer(&arg);
      ast.dumper->dumpSourceRange(arg.range);

      if (arg.implicit)
        ast.out << " implicit";

      switch (arg.value) {
      case pp_callback::CVK_NotEvaluated:
        ast.out << " NotEvaluated";
        break;
      case pp_callback::CVK_False:
        ast.out << " False";
        break;
      case pp_callback::CVK_True:
        ast.out << " True";
        break;
      }
    }

    void operator()(const if_directive::block &arg) {
      ast.out << "CompoundPPStmt";
      ast.dumper->dumpPointer(&arg);
      ast.dumper->dumpSourceRange(arg.range);
    }

    void operator()(const if_directive::defined &arg) {
      ast.out << "DefinedPPOperator";
      ast.dumper->dumpPointer(&arg);
      ast.dumper->dumpSourceRange(arg.range);
      ast.out << ' ';
      ast.dumper->dumpLocation(arg.tok_loc);
      ast.out << ' ';
      ast.dump_name(arg.id);
      ast.dumper->dumpPointer(arg.mi);
    }

    void operator()(const expansion::node *node) { ast.dump_expansion(*node); }

    void operator()(const directive_inclusion &arg) {}
  };

  void dump_directive(const directive_node &node) {
    dumper->AddChild([=, this] {
      std::visit(directive_dumper{*this}, directives[node.i]);
      for (auto child : node.children) {
        dump_directive(directive_nodes[child]);
      }
    });
  }

  void dump_token_expansion() {
    assert(!tokens.empty());
    assert(tokens.back().getKind() == tok::eof);

    auto &sm = pp.getSourceManager();
    SourceLocation last_loc;
    SourceLocation last_expansion_loc;
    int index_of_last_expansion_loc = -1;
    unsigned indexable_tokens = 0;
    unsigned indexed_tokens = 0;

    for (unsigned i = 0, e = tokens.size(); i < e; ++i) {
      syntax::Token token(tokens[i]);
      auto loc = token.location();
      auto kind = token.kind();
      const Decl *d = nullptr;

      if (kind == tok::identifier) {
        ++indexable_tokens;

        if (loc.isFileID()) {
          auto v = find(loc, FIND_OPTION_EQUAL);
          if ((d = v ? v->get_decl() : nullptr)) {
            ++indexed_tokens;
            out << "#TOK-DECL:";
            dumper->dumpLocation(loc);
            dumper->dumpPointer(d);
            out << '\n';
          }
        }
      }

      if (loc.isFileID() && kind != tok::eof) {
        out << "#TOK-KIND:";
        dumper->dumpSourceRange({loc, token.endLocation()});
        auto name = semantic_token::token_name(kind);
        out << " \"" << name.first << ' '
            << (d ? d->getDeclKindName() : name.second) << "\"\n";
      }

      auto expansion_loc = sm.getExpansionLoc(loc);
      if (expansion_loc != last_expansion_loc) {
        if (last_loc.isMacroID()) {
          auto v = find(last_expansion_loc, FIND_OPTION_EQUAL);
          auto d = v ? v->get_expanded_decl() : nullptr;
          int j = i - 1;

          while (d && j > index_of_last_expansion_loc) {
            syntax::Token t(tokens[j]);
            if (t.kind() == tok::identifier) {
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
          // ^~~expanded_decl           ^~~macro_expansion
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

  template <typename T, typename... U> void add_directive(U &&... args) {
    directives.push_back(T{std::forward<U>(args)...});
    add_directive();
  }

  void add_expansion() {
    assert(expanding_stack.empty());

    unsigned macro = last_expansion.first;
    unsigned token = last_expansion.second;
    auto expected_macro_size = macros.size() - macro;
    auto expected_token_size = macro_tokens.size() - token;

    expansion exp;
    exp.pool.reserve(expected_macro_size + expected_token_size);

    index_macro(macro, macros.size());
    make_expansion(-1, macro, macros.size(), token, exp.root, exp.pool);
    assert(exp.pool.size() == expected_macro_size + expected_token_size);

    if (!exp.pool.empty()) {
      last_expansion.first = macros.size();
      last_expansion.second = macro_tokens.size();

      if (defined_queue.empty()) {
        for (auto node : exp.root.children)
          add_directive<expansion::node *>(node);
      } else {
        std::vector<std::tuple<SourceLocation, unsigned, bool>> order;
        order.reserve(exp.root.children.size() + defined_queue.size());
        for (size_t i = 0; i < exp.root.children.size(); ++i) {
          auto node = exp.root.children[i];
          assert(!node->token);
          order.emplace_back(macros[node->i].range.getBegin(), i, true);
        }
        for (size_t i = 0; i < defined_queue.size(); ++i) {
          order.emplace_back(defined_queue[i].tok_loc, i, false);
        }

        std::sort(order.begin(), order.end(), [](auto &a, auto &b) {
          return std::get<0>(a) < std::get<0>(b);
        });

        for (auto &item : order) {
          auto i = std::get<1>(item);
          if (std::get<2>(item))
            add_directive<expansion::node *>(exp.root.children[i]);
          else
            add_directive<if_directive::defined>(std::move(defined_queue[i]));
        }
      }

      expansions.push_back(std::move(exp));
    } else {
      for (auto &item : defined_queue)
        add_directive<if_directive::defined>(std::move(item));
    }

    defined_queue.clear();
  }

  void make_expansion(int parent, unsigned begin, unsigned end, unsigned &k,
                      expansion::node &host,
                      std::vector<expansion::node> &pool) {
    for (auto i = begin; i < end; ++i) {
      make_expansion(parent, k, host, pool);
      auto &node = pool.emplace_back(i, false);
      host.children.emplace_back(&node);
      make_expansion(i, i + 1, macros[i].remote + 1, k, node, pool);
      i = macros[i].remote;
    }

    make_expansion(parent, k, host, pool);
  }

  void make_expansion(int at, unsigned &k, expansion::node &host,
                      std::vector<expansion::node> &pool) {
    unsigned n = macro_tokens.size();
    while (k < n && macro_tokens[k].first == at) {
      host.children.emplace_back(&pool.emplace_back(k, true));
      ++k;
    }
  }

  void dump_expansion(const expansion::node &node) {
    if (node.token)
      dump_token(macro_tokens[node.i].second,
                 macros[macro_tokens[node.i].first].info);
    else
      dump_macro(macros[node.i]);

    for (auto child : node.children)
      dumper->AddChild([child, this] { dump_expansion(*child); });
  }

  void dump_macro(const macro_expansion &macro) {
    out << "MacroExpansion";
    dumper->dumpPointer(macro.info);
    dumper->dumpSourceRange(macro.range);
    out << ' ';
    dump_name(macro.token.getIdentifierInfo());
    if (macro.fast)
      out << " fast";
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
    auto token_length = dump_token_content(token);
    out.escape(0);
    out << '/';

    SourceLocation origin_loc;
    if (macro && macro != provider) {
      out << " macro";
      dumper->dumpPointer(macro);

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

    if (is_arg) {
      out << " arg";
      add_tok({loc, loc.getLocWithOffset(token_length)}, token.getKind());
    }
  }

  void dump_name(const IdentifierInfo *id) { out << "\u200B" << id->getName(); }

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
  llvm::SmallVector<if_directive::defined, 8> defined_queue;
  llvm::DenseMap<const MacroInfo *, const IdentifierInfo *> defined_map;
  std::vector<macro_test> tests;
  std::vector<macro_expansion> macros;
  std::vector<std::pair<unsigned, Token>> macro_tokens; // with replacements
  std::vector<unsigned> trivial_expansions;             // without replacements
  std::vector<expansion> expansions;
  std::pair<unsigned, unsigned> last_expansion;
  std::vector<std::variant<def_directive, if_directive, if_directive::cond,
                           if_directive::block, if_directive::defined,
                           directive_inclusion, expansion::node *>>
      directives;
  unsigned dir; // index of the present directive_node
  std::vector<directive_node> directive_nodes;
  llvm::DenseMap<FileID, std::map<unsigned, index_value_t>> indices;
  std::vector<Token> tokens;
  std::vector<SourceRange> inactive_regions;
  std::vector<const void *> exported_symbols;
  std::vector<semantic_token> semantic_tokens;
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
    compiler.getLangOpts().CommentOpts.ParseAllComments = true;
    compiler.getLangOpts().RetainCommentsFromSystemHeaders = true;

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
