LEX= flex
YACC= bison
CFLAGS= -MMD -Werror -std=gnu11
LDFLAGS= -lfl -lsqlite3
LEXFLAGS= 
YACCFLAGS= -Werror

ifdef RELEASE
CFLAGS+= -O2 -DNDEBUG
else
CFLAGS+= -g -O0
endif

ifdef USE_TREE_SITTER
TREE_SITTER_DIR?= ${HOME}/tree-sitter
TREE_SITTER_C_DIR?= ${HOME}/tree-sitter-c
TREE_SITTER_CFLAGS= -I${TREE_SITTER_DIR}/lib/include
TREE_SITTER_LDFLAGS= ${TREE_SITTER_DIR}/libtree-sitter.a
CFLAGS+= -DUSE_TREE_SITTER ${TREE_SITTER_CFLAGS}
LDFLAGS+= ${TREE_SITTER_LDFLAGS}
SRCS+= ${TREE_SITTER_C_DIR}/src/parser.c
endif

ifdef USE_TEST
CFLAGS+= -DUSE_TEST
SRCS+= test.c
endif

SRCS+= sql.c main.c
GENHDRS+= parse.h
GENSRCS+= parse.c scan.c

build: caq

OBJS= ${SRCS:.c=.o}
-include ${OBJS:.o=.d}

test: test-parse test-mem test-query

test-parse: build
	@for i in samples/*.gz; do printf "\r%-30s" $$i; zcat $$i | ./caq || exit 1; done
	@printf "\r"

test-mem: build
	@zcat samples/00000001.gz | valgrind ./caq

test-query: build
	@shellspec

test-sql: build
	@./caq -x samples/main.ast | sqlite3

caq: ${OBJS} ${GENSRCS}
	${CC} -o $@ ${CFLAGS} $^ ${LDFLAGS}

parse.h: parse.c
parse.c: parse.y
	${YACC} -d $< ${YACCFLAGS} -o $@

scan.c: scan.l
	${LEX} --header-file=$(basename $<).h -o $@ ${LEXFLAGS} $<

clean:
	rm -f *.out *.o ${GENSRCS} ${GENHDRS}

.PHONY: build test test-parse test-query clean