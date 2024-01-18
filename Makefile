LEX= flex
YACC= bison
CFLAGS= -MMD -Werror -std=gnu11 -g -O0
LDFLAGS= -lfl
LEXFLAGS= 
YACCFLAGS= -Werror

SRCS=
GENSRCS= parse.c scan.c

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

parse.c: parse.y
	${YACC} -d $< ${YACCFLAGS} -o $@

scan.c: scan.l
	${LEX} --header-file=$(basename $<).h -o $@ ${LEXFLAGS} $<

clean:
	rm -f *.out *.o

.PHONY: build test test-parse test-query clean