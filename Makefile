LEX= flex
YACC= bison
CFLAGS=
LDFLAGS= -lfl
LEXFLAGS= 
YACCFLAGS= -Werror

run: test
	@for i in samples/*; do echo ==== $$i ====; zcat $$i | ./$< || exit 1; done

test: token.tab.c lex.yy.c test.c
	${CC} -o $@ ${LDFLAGS} ${CFLAGS} $^

token.tab.c: token.y
	${YACC} -d $< ${YACCFLAGS} -o $@

lex.yy.c: lex.l
	${LEX} -o $@ --header-file=$(basename $@).h ${LEXFLAGS} $<

clean:
	rm -f token *.o *.yy.* *.tab.*

.PHONY: clean