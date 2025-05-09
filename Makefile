LEX= flex
YACC= bison
LEXFLAGS=
YACCFLAGS= -Werror

CC= cc
CXX= c++
CFLAGS+= -MMD -Werror -std=gnu23
CXXFLAGS+= -std=c++20 -fno-exceptions -fno-rtti
LDFLAGS= -lfl -lsqlite3

ifdef RELEASE
CPPFLAGS+= -DNDEBUG
CFLAGS+= -O2
CXXFLAGS+= -O2
else
CFLAGS+= -g -O0 -DREADER_JS='"./reader.bundle.js"'
CXXFLAGS+= -g -O0
endif

ifdef USE_TEST
CPPFLAGS+= -DUSE_TEST
# MUST be the first linked one
SRCS:= test.c
endif

ifdef USE_TOGGLE
CPPFLAGS+= -DUSE_TOGGLE
# MUST be the first linked one
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

GENHDRS+= parse.h scan.h reader.bundle.js
GENSRCS+= parse.c scan.c
SRCS+= array.c string.c string_set.c store.c render.c util.c murmur3.c build.c main.c ${GENSRCS}

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

reader.bundle.js: reader.js rollup.config.js
	npm run build

clean:
	rm -f caq *.output *.out *.o *.d ${GENSRCS} ${GENHDRS}

.PHONY: build test test-parse test-query test-fun test-mem clean
