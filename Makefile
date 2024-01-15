LEX= flex
YACC= bison
CFLAGS= -MMD -Werror -std=gnu11 -g
LDFLAGS= -lfl
LEXFLAGS= 
YACCFLAGS= -Werror

SRCS= ast.c
GENSRCS= parse.tab.c lex.yy.c

build: test.out

OBJS= ${SRCS:.c=.o}
-include ${OBJS:.o=.d}

test: test-parse test-query

test-parse: build
	@for i in samples/*.gz; do printf "\r%-30s" $$i; zcat $$i | ./test.out || exit 1; done
	@printf "\r"

test-query: build
	@shellspec

test.out: ${GENSRCS} ${OBJS} test.c
	${CC} -o $@ ${CFLAGS} $^ ${LDFLAGS}

parse.tab.c: parse.y
	${YACC} -d $< ${YACCFLAGS} -o $@

lex.yy.c: lex.l
	${LEX} -o $@ --header-file=$(basename $@).h ${LEXFLAGS} $<

clean:
	rm -f *.out *.o *.yy.* *.tab.*

.PHONY: build test test-parse test-query clean