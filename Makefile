LEX= flex
YACC= bison
LEXFLAGS= 
YACCFLAGS= -Werror

CC= cc
CXX= c++
CFLAGS= -MMD -Werror -std=gnu11
CXXFLAGS= -Werror -std=c++20 -fno-exceptions -fno-rtti
LDFLAGS= -lfl -lsqlite3

ifdef RELEASE
CPPFLAGS+= -DNDEBUG
CFLAGS+= -O2
else
CFLAGS+= -g -O0
endif

ifdef USE_TEST
CPPFLAGS+= -DUSE_TEST
# Must be the first linked one
SRCS:= test.c
endif

ifdef USE_TREE_SITTER
TREE_SITTER_DIR?= ${HOME}/tree-sitter
TREE_SITTER_C_DIR?= ${HOME}/tree-sitter-c
TREE_SITTER_CFLAGS= -I${TREE_SITTER_DIR}/lib/include
TREE_SITTER_LDFLAGS= ${TREE_SITTER_DIR}/libtree-sitter.a
CPPFLAGS+= -DUSE_TREE_SITTER
CFLAGS+= ${TREE_SITTER_CFLAGS}
LDFLAGS+= ${TREE_SITTER_LDFLAGS}
SRCS+= ${TREE_SITTER_C_DIR}/src/parser.c
endif

ifdef USE_CLANG_TOOL
LLVM_DIR?= /usr/lib/llvm-17
CPPFLAGS+= -DUSE_CLANG_TOOL
CXXFLAGS+= -I${LLVM_DIR}/include
LDFLAGS+= -L${LLVM_DIR}/lib -lclang-cpp -lLLVM -lstdc++
SRCS+= remark.cc
endif

SRCS+= print.c sql.c main.c
GENHDRS+= parse.h scan.h
GENSRCS+= parse.c scan.c

build: caq

OBJS:= $(addsuffix .o,$(basename ${SRCS}))
-include ${OBJS:.o=.d}

test: test-parse test-mem test-query test-fun

test-parse: build
	@for i in samples/*.gz; do printf "\r%-30s" $$i; zcat $$i | ./caq || exit 1; done
	@printf "\r"

test-mem: build
	@zcat samples/00000001.gz | valgrind ./caq

test-query: build
	@shellspec

test-fun: build
	@./caq -t

caq: ${OBJS} ${GENSRCS}
	${CC} -o $@ ${CPPFLAGS} ${CFLAGS} $^ ${LDFLAGS}

parse.h: parse.c
parse.c: parse.y
	${YACC} -d $< ${YACCFLAGS} -o $@

scan.h: scan.c
scan.c: scan.l
	${LEX} --header-file=$(basename $<).h -o $@ ${LEXFLAGS} $<

clean:
	rm -f *.out *.o ${GENSRCS} ${GENHDRS}

.PHONY: build test test-parse test-query test-fun test-mem clean
