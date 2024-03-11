#include "remark.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/TextNodeDumper.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
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
using namespace clang::ast_matchers;

class expanded_decl {
public:
  expanded_decl(SourceLocation loc, Decl *decl) : loc(loc), decl(decl) {
    assert(loc.isMacroID());
  }

  SourceLocation get_location() const { return loc; }

  Decl *get_decl() const { return decl; }

  expanded_decl *get_previous() const { return prev; }

  void set_previous(expanded_decl *prev) { this->prev = prev; }

private:
  SourceLocation loc;
  Decl *decl;
  expanded_decl *prev;
};

class index_value_t {
public:
  explicit index_value_t(MacroInfo *p) : p(p) {
    static_assert(alignof(MacroInfo) >= 4 && alignof(MacroInfo) % 4 == 0);
    assert(is_macro());
  }

  explicit index_value_t(Decl *p) : p((char *)p + 1) {
    static_assert(alignof(Decl) >= 4 && alignof(Decl) % 4 == 0);
    assert(is_decl());
  }

  explicit index_value_t(expanded_decl *p) : p((char *)p + 2) {
    static_assert(alignof(expanded_decl) >= 4 &&
                  alignof(expanded_decl) % 4 == 0);
    assert(is_expanded_decl());
  }

  int kind() const { return ((std::uintptr_t)p & 3U); }

  bool is_macro() const { return kind() == 0; }

  bool is_decl() const { return kind() == 1; }

  bool is_expanded_decl() const { return kind() == 2; }

  MacroInfo *get_macro() const {
    assert(is_macro());
    return (MacroInfo *)p;
  }

  Decl *get_decl() const {
    assert(is_decl());
    return (Decl *)((char *)p - 1);
  }

  expanded_decl *get_expanded_decl() const {
    assert(is_expanded_decl());
    return (expanded_decl *)((char *)p - 2);
  }

  void *get_raw() const {
    switch (kind()) {
    case 0:
      return p;
    case 1:
      return (char *)p - 1;
    case 2:
      return (char *)p - 2;
    default:
      assert("Never reach here");
      return nullptr;
    }
  }

  bool operator==(index_value_t other) const {
    return get_raw() == other.get_raw();
  }

private:
  void *p;
};

auto var = varDecl(anything()).bind("var");
auto field = fieldDecl(anything()).bind("field");
auto func = functionDecl(anything()).bind("func");

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

template <size_t N> struct literal {
  constexpr literal(const char (&str)[N]) { std::copy_n(str, N, value); }
  char value[N];
};

class ast_consumer final : public ASTConsumer {
public:
  ast_consumer(std::unique_ptr<raw_line_ostream> os, Preprocessor &pp)
      : out(*os), pp(pp), os(std::move(os)), var_cb(*this), field_cb(*this),
        func_cb(*this) {
    finder.addMatcher(var, &var_cb);
    finder.addMatcher(field, &field_cb);
    finder.addMatcher(func, &func_cb);
    impl = finder.newASTConsumer();
  }

protected:
  void Initialize(ASTContext &context) override {
    dumper.emplace(out, context, false);
    pp.addPPCallbacks(std::make_unique<pp_callback>(*this));
    pp.setTokenWatcher([this](auto &token) { on_token_lexed(token); });
    impl->Initialize(context);
  }

  bool HandleTopLevelDecl(DeclGroupRef dg) override {
    return impl->HandleTopLevelDecl(dg);
  }

  void HandleInterestingDecl(DeclGroupRef dg) override {
    impl->HandleInterestingDecl(dg);
  }

  void HandleTopLevelDeclInObjCContainer(DeclGroupRef dg) override {
    impl->HandleTopLevelDeclInObjCContainer(dg);
  }

  void HandleTranslationUnit(ASTContext &ctx) override {
    auto &sm = ctx.getSourceManager();
    const auto file = sm.getFileEntryRefForID(sm.getMainFileID());
    const auto &filename = file->getName();
    char cwd[PATH_MAX];

    out << "#TU:" << filename << '\n';
    out << "#CWD:" << getcwd(cwd, sizeof(cwd)) << '\n';
    impl->HandleTranslationUnit(ctx);
    dump_token_expansion();
  }

  bool shouldSkipFunctionBody(Decl *d) override {
    return impl->shouldSkipFunctionBody(d);
  }

private:
  template <typename T, literal S>
  class match_callback : public MatchFinder::MatchCallback {
  public:
    explicit match_callback(ast_consumer &ast) : ast(ast) {}

    void run(const MatchFinder::MatchResult &result) override {
      if (auto p = result.Nodes.getNodeAs<T>(S.value)) {
        auto d = value(p->getType().getTypePtr());
        if (d && !d->isImplicit()) {
          if constexpr (std::is_same_v<T, FunctionDecl>) {
            auto range = p->getReturnTypeSourceRange();
            index(range.getBegin(), d);
          } else if (auto ti = p->getTypeSourceInfo()) {
            auto type_loc = ti->getTypeLoc();
            index(type_loc.getBeginLoc(), d);
          } else {
            assert(p->getLocation().isInvalid());
          }
        }

        bool is_unnamed = false;
        if constexpr (std::is_same_v<T, FieldDecl>) {
          is_unnamed = p->isUnnamedBitfield() || p->isAnonymousStructOrUnion();
        }
        if (!is_unnamed)
          index(p->getLocation(),
                const_cast<Decl *>(static_cast<const Decl *>(p)));
      }
    }

  private:
    ast_consumer &ast;

    Decl *value(const Type *p) {
      if (auto t = p->getAs<TypedefType>()) {
        return t->getDecl();
      }

      if (auto t = p->getAs<TagType>()) {
        return t->getDecl();
      }

      if (auto t = p->getAs<FunctionType>()) {
        // The parameters would be walked via `var` matcher.
        return value(t->getReturnType().getTypePtr());
      }

      if (auto t = p->getAs<PointerType>()) {
        return value(t->getPointeeType().getTypePtr());
      }

      if (auto t = p->getArrayElementTypeNoTypeQual()) {
        return value(t);
      }

      return nullptr;
    }

    void index(SourceLocation loc, Decl *p) {
      auto &sm = ast.pp.getSourceManager();
      if (loc.isMacroID()) {
        ast.index(sm.getExpansionLoc(loc),
                  index_value_t(new expanded_decl(loc, p)));
      } else if (loc.isValid()) {
        ast.index(loc, index_value_t(p));
      }
    }
  };

  class pp_callback : public PPCallbacks {
  public:
    explicit pp_callback(ast_consumer &ast) : ast(ast) {}

    void MacroDefined(const Token &token, const MacroDirective *md) override {
      auto mi = md->getMacroInfo();
      auto &out = ast.out;
      auto &dumper = ast.dumper;

      ast.dump_kind(md->getKind());
      out << "Directive";
      dumper->dumpPointer(md);
      dumper->dumpSourceRange(
          {mi->getDefinitionLoc(), mi->getDefinitionEndLoc()});
      out << " '";
      out << token.getIdentifierInfo()->getName();
      out << "'";
      dumper->dumpPointer(mi);
      dumper->AddChild([=, this] { ast.dump_macro(mi); });
    }

    void MacroExpands(const Token &token, const MacroDefinition &def,
                      SourceRange range, const MacroArgs *args) override {
      int parent =
          ast.expanding_stack.empty() ? -1 : ast.expanding_stack.back();
      unsigned remote = ast.macros.size();
      ast.macros.emplace_back(token, def.getMacroInfo(), range, parent, remote);
      ast.expanding_stack.push_back(remote);

      // Update remote field up to the root
      while (parent != -1) {
        auto &macro = ast.macros[parent];
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
      on_if(loc, range, value, {});
    }

    void Elif(SourceLocation loc, SourceRange range, ConditionValueKind value,
              SourceLocation if_loc) override {
      on_if(loc, range, value, if_loc);
    }

  private:
    void on_if(SourceLocation loc, SourceRange range, ConditionValueKind value,
               SourceLocation if_loc) {
      ast.dump_macro_expansion();
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
    if (mi->isFunctionLike()) {
      dumper->AddChild([=, this] {
        auto i = mi->param_begin(), e = mi->param_end();
        out << "ParameterList";
        dumper->dumpPointer(i);
        out << " '";

        if (!mi->param_empty()) {
          for (; i + 1 != e; ++i) {
            out << (*i)->getName();
            out << ' ';
          }

          // Last argument.
          if ((*i)->getName() == "__VA_ARGS__")
            out << "...";
          else
            out << (*i)->getName();
        }

        if (mi->isGNUVarargs())
          out << "..."; // foo(x...)

        out << "'";
      });
    }
    if (!mi->tokens_empty()) {
      dumper->AddChild([=, this] {
        out << "ReplacementTokens";
        dumper->dumpPointer(mi->tokens_begin());
        for (const auto &t : mi->tokens()) {
          dumper->AddChild([&t, this] { dump_token(t); });
        }
      });
    }
  }

  void on_token_lexed(const Token &token) {
    // Collect all tokens to dump in later.
    if (!token.isAnnotation())
      tokens.push_back(syntax::Token(token));

    auto loc = token.getLocation();
    if (loc.isFileID()) {
      dump_macro_expansion();
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
    case tok::kw_struct:
    case tok::kw_union:
    case tok::identifier:
      return true;
    default:
      return false;
    }
  }

  void dump_token_expansion() {
    assert(!tokens.empty());
    assert(tokens.back().kind() == tok::eof);
    auto &sm = pp.getSourceManager();
    SourceLocation last_loc;
    SourceLocation last_expansion_loc;
    unsigned index_of_last_expansion_loc = -1;
    for (unsigned i = 0, e = tokens.size(); i < e; ++i) {
      auto loc = tokens[i].location();
      if (loc.isFileID() && indexable(tokens[i])) {
        // TODO:
      }

      auto expansion_loc = sm.getExpansionLoc(loc);
      if (expansion_loc != last_expansion_loc) {
        if (last_loc.isMacroID()) {
          auto v = find(last_expansion_loc);
          auto d = v ? v->get_expanded_decl() : nullptr;
          while (d) {
            d->get_location().print(out, sm);
            out << ' ' << d->get_decl() << '\n';
            d = d->get_previous();
          }
          unsigned j = i - 1;
          while (j > index_of_last_expansion_loc) {
            auto &t = tokens[j];
            if (indexable(t)) {
              out << t.dumpForTests(sm);
              out << ' ';
              t.location().print(out, sm);
              out << '\n';
            }

            --j;
          }
        }

        last_loc = loc;
        last_expansion_loc = expansion_loc;
        index_of_last_expansion_loc = i - 1;
      }
    }
  }

  void dump_macro_expansion() {
    assert(expanding_stack.empty());

    unsigned k = 0;
    macro_full_expansion_node root;
    std::vector<macro_full_expansion_node> pool;
    pool.reserve(expansions.size() + macros.size());

    index_macro();
    make_macro_full_expansion(0, macros.size(), k, root, pool);
    assert(pool.size() == expansions.size() + macros.size());
    for (auto &item : root.children) {
      dump_macro_expansion(*item);
    }

    expansions.clear();
    macros.clear();
  }

  void make_macro_full_expansion(unsigned begin, unsigned end, unsigned &k,
                                 macro_full_expansion_node &host,
                                 std::vector<macro_full_expansion_node> &pool) {
    int parent = begin - 1;

    for (auto i = begin; i < end; ++i) {
      make_macro_full_expansion_token(parent, k, host, pool);
      auto &node = pool.emplace_back(i, false);
      host.children.emplace_back(&node);
      make_macro_full_expansion(i + 1, macros[i].remote + 1, k, node, pool);
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

  void dump_macro_expansion(const macro_full_expansion_node &node) {
    dumper->AddChild([&, this] {
      if (node.token)
        dump_token(expansions[node.i].second,
                   macros[expansions[node.i].first].info);
      else
        dump_macro_expansion(macros[node.i]);

      for (auto &item : node.children) {
        dump_macro_expansion(*item);
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

  const index_value_t *find(SourceLocation loc) const {
    assert(loc.isFileID());

    FileID fid;
    unsigned offset;
    std::tie(fid, offset) = pp.getSourceManager().getDecomposedLoc(loc);
    if (auto i = indices.find(fid); i != indices.end()) {
      auto j = i->second.lower_bound(offset);
      if (j != i->second.end())
        return &j->second;
    }

    return nullptr;
  }

  void index_macro() {
    for (auto &macro : macros) {
      auto end_loc = macro.info->getDefinitionEndLoc();
      if (end_loc.isValid())
        index(end_loc, index_value_t(macro.info));
      else
        assert(macro.info->isBuiltinMacro());
    }
  }

  const MacroInfo *find_macro(SourceLocation loc) const {
    if (auto v = find(loc)) {
      return v->get_macro();
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
      if (macro) {
        assert(
            SourceRange(macro->getDefinitionLoc(), macro->getDefinitionEndLoc())
                .fullyContains({spelling_loc, spelling_loc}));
      } else {
        assert(spelling_loc.isFileID() && sm.isMacroArgExpansion(loc));
        is_arg = true;
      }
    }

    out << "'";
    out.escape('\'');
    dump_token_content(token);
    out.escape(0);
    out << "'";

    dumper->dumpSourceRange({loc});
    if (loc != raw_loc) {
      out << ' ';
      dumper->dumpLocation(raw_loc);
    }

    if (token.hasLeadingSpace())
      out << " hasLeadingSpace";

    if (token.stringifiedInMacro())
      out << " stringified";
    else if (loc != raw_loc)
      out << " paste";

    if (is_arg)
      out << " arg";
    else if (macro != provider)
      out << " macro " << macro;
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
  match_callback<VarDecl, "var"> var_cb;
  match_callback<FieldDecl, "field"> field_cb;
  match_callback<FunctionDecl, "func"> func_cb;
  ast_matchers::MatchFinder finder;
  std::unique_ptr<ASTConsumer> impl;
  std::optional<TextNodeDumper> dumper;
  llvm::SmallVector<unsigned, 8> expanding_stack;
  llvm::SmallVector<macro_expansion_node, 8> macros;
  llvm::SmallVector<std::pair<unsigned, Token>, 32> expansions;
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
