YACC= bison
LEX= flex
LDFLAGS= 

parse: token.y lex.l
	${YACC} -d token.y
	${LEX} lex.l
	${CC} -o $@ ${LDFLAGS} ${CFLAGS} token.tab.c lex.yy.c parse.c

clean:
	rm -f token *.o

.PHONY: clean