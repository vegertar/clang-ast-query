#include "remark.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Tooling/Tooling.h>

#include <unistd.h>

namespace {

using namespace clang;
using namespace clang::ast_matchers;

auto matcher = varDecl(anything()).bind("var");

class raw_line_ostream : public raw_ostream {
 public:
  explicit raw_line_ostream(
      int (*parse)(char *line, size_t n, size_t cap, void *data) = nullptr,
      void *data = nullptr,
      size_t cap = BUFSIZ)
      : pos(0), parse(parse), data(data) {
    line.reserve(cap);
    SetUnbuffered();
  }

  void write_impl(const char *ptr, size_t size) override {
    pos += size;

    size_t i = 0;
    while (i < size) {
      if (ptr[i++] == '\n') {
        // with 2 more slots to work with parse_line()
        size_t cap = line.size() + i + 2;
        if (line.capacity() < cap) line.reserve(cap);
        line.append(ptr, i);
        if (parse) parse(line.data(), line.size(), line.capacity(), data);

        ptr += i;
        size -= i;
        i = 0;
        line.clear();
      }
    }

    if (size) line.append(ptr, size);
  }

  uint64_t current_pos() const override { return pos; }

 private:
  std::string line;
  uint64_t pos;
  int (*parse)(char *line, size_t n, size_t cap, void *data);
  void *data;
};

class match_callback : public MatchFinder::MatchCallback {
 public:
  explicit match_callback(raw_ostream &out) : out(out) {}

  void run(const MatchFinder::MatchResult &result) override {
    if (auto var = result.Nodes.getNodeAs<VarDecl>("var")) {
      if (auto type = decl(var->getType().getTypePtr())) {
        out << "#VAR-TYPE:" << var << ' ' << type << '\n';
      }
    }
  }

  Decl *decl(const Type *p) {
    Decl *type = nullptr;
    if (auto t = p->getAs<TypedefType>()) {
      type = t->getDecl();
    } else if (auto t = p->getAs<TagType>()) {
      type = t->getDecl();
    } else if (auto t = p->getAs<PointerType>()) {
      type = decl(t->getPointeeType().getTypePtr());
    }
    return type;
  }

 private:
  raw_ostream &out;
};

class ast_consumer final : public ASTConsumer {
 public:
  ast_consumer(std::unique_ptr<raw_ostream> os)
      : out(os ? *os : llvm::outs()), os(std::move(os)), cb(out) {
    finder.addMatcher(matcher, &cb);
    impl = finder.newASTConsumer();
  }

 protected:
  void Initialize(ASTContext &context) override {
    return impl->Initialize(context);
  }

  bool HandleTopLevelDecl(DeclGroupRef dg) override {
    return impl->HandleTopLevelDecl(dg);
  }

  void HandleInterestingDecl(DeclGroupRef dg) override {
    return impl->HandleInterestingDecl(dg);
  }

  void HandleTopLevelDeclInObjCContainer(DeclGroupRef dg) override {
    return impl->HandleTopLevelDeclInObjCContainer(dg);
  }

  void HandleTranslationUnit(ASTContext &ctx) override {
    auto &sm = ctx.getSourceManager();
    const auto file = sm.getFileEntryForID(sm.getMainFileID());
    const auto &filename = file->getName();
    char cwd[PATH_MAX];

    out << "#TU:" << filename << '\n';
    out << "#CWD:" << getcwd(cwd, sizeof(cwd)) << '\n';
    return impl->HandleTranslationUnit(ctx);
  }

  bool shouldSkipFunctionBody(Decl *d) override {
    return impl->shouldSkipFunctionBody(d);
  }

 private:
  raw_ostream &out;
  std::unique_ptr<raw_ostream> os;
  match_callback cb;
  ast_matchers::MatchFinder finder;
  std::unique_ptr<ASTConsumer> impl;
};

std::unique_ptr<ASTConsumer> make_ast_dumper(
    std::unique_ptr<raw_ostream> os,
    CompilerInstance &compiler,
    std::string_view in_file) {
  const auto &opts = compiler.getFrontendOpts();
  return CreateASTDumper(std::move(os),
                         opts.ASTDumpFilter,
                         opts.ASTDumpDecls,
                         opts.ASTDumpAll,
                         opts.ASTDumpLookups,
                         opts.ASTDumpDeclTypes,
                         opts.ASTDumpFormat);
}

std::unique_ptr<ASTConsumer> make_ast_consumer(
    std::unique_ptr<raw_ostream> os,
    CompilerInstance &compiler,
    std::string_view in_file) {
  return std::make_unique<ast_consumer>(std::move(os));
}

class frontend_action : public ASTFrontendAction {
 public:
  frontend_action() = default;
  frontend_action(
      int (*parse_line)(char *line, size_t n, size_t cap, void *data),
      void *data)
      : parse_line(parse_line), data(data) {}

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &compiler,
                    llvm::StringRef in_file) override {
    std::vector<std::unique_ptr<ASTConsumer>> v;
    v.push_back(make_ast_dumper(std::make_unique<raw_line_ostream>(parse_line, data),
                                compiler,
                                in_file));
    v.push_back(make_ast_consumer(std::make_unique<raw_line_ostream>(parse_line, data),
                                  compiler,
                                  in_file));
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

int remark(const char *code,
           size_t size,
           const char *filename,
           char **opts, int n,
           int (*parse_line)(char *line, size_t n, size_t cap, void *data),
           void *data) {
  std::vector<std::string> args(opts, opts + n);
  args.push_back("-Xclang");
  args.push_back("-ast-dump");
  args.push_back("-fno-color-diagnostics");

  if (!filename) filename = "input.c";
  if (!parse_line) parse_line = print_line;

  return clang::tooling::runToolOnCodeWithArgs(
      std::make_unique<frontend_action>(parse_line, data),
      std::string_view{code, size},
      args,
      filename) ? 0 : -1;
}
