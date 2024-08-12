LEX= flex
YACC= bison
LEXFLAGS= 
YACCFLAGS= -Werror

CC= cc
CXX= c++
CFLAGS= -MMD -Werror -std=gnu11
CXXFLAGS= -std=c++20 -fno-exceptions -fno-rtti
LDFLAGS= -lfl -lsqlite3

ifdef RELEASE
CPPFLAGS+= -DNDEBUG
CFLAGS+= -O2
CXXFLAGS+= -O2
else
CFLAGS+= -g -O0
CXXFLAGS+= -g -O0
endif

ifdef USE_TEST
CPPFLAGS+= -DUSE_TEST
# Must be the first linked one
SRCS:= test.c
endif

ifdef USE_TOGGLE
CPPFLAGS+= -DUSE_TOGGLE
# Must be the first linked one
SRCS:= test.c
endif

ifdef USE_CLANG_TOOL
CPPFLAGS+= -DUSE_CLANG_TOOL
LLVM_PROJECT_DIR?= ${HOME}/llvm-project
LLVM_PROJECT_BUILD_DIR?= ${LLVM_PROJECT_DIR}/build
CXXFLAGS+= \
	-I${LLVM_PROJECT_DIR}/llvm/include \
	-I${LLVM_PROJECT_DIR}/clang/include \
	-I${LLVM_PROJECT_BUILD_DIR}/include \
	-I${LLVM_PROJECT_BUILD_DIR}/tools/clang/include
LDFLAGS+= -L${LLVM_PROJECT_BUILD_DIR}/lib -lclang-cpp -lstdc++
SRCS+= remark.cc
endif

GENHDRS+= parse.h scan.h
GENSRCS+= parse.c scan.c
SRCS+= print.c array.c string.c string_set.c sql.c html.c util.c murmur3.c main.c ${GENSRCS}

build: ${GENHDRS} caq

OBJS:= $(addsuffix .o,$(basename ${SRCS}))
-include ${OBJS:.o=.d}

test: test-parse test-mem test-query test-fun

test-parse: build
	@for i in samples/nginx/*.gz; do printf "\r%-30s" $$i; zcat $$i | ./caq || exit 1; done
	@printf "\r"

test-mem: build
	@zcat samples/nginx/00000001.gz | valgrind ./caq

test-query: build
	@shellspec

test-fun: build
	@./caq -t

caq: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS}

parse.h: parse.c
parse.c: parse.y
	${YACC} -d $< ${YACCFLAGS} -o $@

scan.h: scan.c
scan.c: scan.l
	${LEX} --header-file=$(basename $<).h -o $@ ${LEXFLAGS} $<

clean:
	rm -f caq *.output *.out *.o *.d ${GENSRCS} ${GENHDRS}

.PHONY: build test test-parse test-query test-fun test-mem clean
