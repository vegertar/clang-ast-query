LEX= flex
YACC= bison
CFLAGS=
LDFLAGS= -lfl
LEXFLAGS= 
YACCFLAGS= -Werror

build: test.out

test: build
	@for i in samples/*; do printf "\r==== $$i ===="; zcat $$i | ./test.out || exit 1; done
	@printf "\r"

test.out: token.tab.c lex.yy.c test.c
	${CC} -o $@ ${LDFLAGS} ${CFLAGS} $^

token.tab.c: token.y
	${YACC} -d $< ${YACCFLAGS} -o $@

lex.yy.c: lex.l
	${LEX} -o $@ --header-file=$(basename $@).h ${LEXFLAGS} $<

clean:
	rm -f *.out *.o *.yy.* *.tab.*

.PHONY: build test clean