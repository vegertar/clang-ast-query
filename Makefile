LEX= flex
YACC= bison
CFLAGS= -MMD -Werror -std=gnu11
LDFLAGS= -lfl -lsqlite3
LEXFLAGS= 
YACCFLAGS= -Werror
SRCS= sql.c
GENSRCS= parse.c scan.c

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

build: test.out

OBJS= ${SRCS:.c=.o}
-include ${OBJS:.o=.d}

test: test-parse test-mem test-query

test-parse: build
	@for i in samples/*.gz; do printf "\r%-30s" $$i; zcat $$i | ./test.out || exit 1; done
	@printf "\r"

test-mem: build
	@zcat samples/00000001.gz | valgrind ./test.out

test-query: build
	@shellspec

test-sql: build
	@./test.out -x samples/main.ast | sqlite3

test.out: ${GENSRCS} ${OBJS} test.c
	${CC} -o $@ ${CFLAGS} $^ ${LDFLAGS}

parse.c: parse.y
	${YACC} -d $< ${YACCFLAGS} -o $@

scan.c: scan.l
	${LEX} --header-file=$(basename $<).h -o $@ ${LEXFLAGS} $<

clean:
	rm -f *.out *.o

.PHONY: build test test-parse test-query clean