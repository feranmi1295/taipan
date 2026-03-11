CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iast -Ilexer -Iparser -Icompiler -Iruntime
LDFLAGS = -lm

SRCS =  lexer/lexer.c       \
        ast/ast.c           \
        parser/parser.c     \
        compiler/analyzer.c \
        compiler/codegen.c  \
        compiler/compiler.c \
        compiler/main.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean test run runtime

all: runtime venom

# Build runtime static library
runtime: runtime/libtaipan.a

runtime/libtaipan.a: runtime/runtime.o
	ar rcs $@ $^
	@echo "✅  libtaipan.a built"

runtime/runtime.o: runtime/runtime.c runtime/runtime.h
	$(CC) $(CFLAGS) -c $< -o $@

# Build venom compiler
venom: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅  venom built"

# Test binaries
test_lexer: lexer/lexer.c ast/ast.c tests/test_lexer.c
	$(CC) $(CFLAGS) -o $@ $^

test_parser: lexer/lexer.c ast/ast.c parser/parser.c tests/test_parser.c
	$(CC) $(CFLAGS) -o $@ $^

test_analyzer: lexer/lexer.c ast/ast.c parser/parser.c compiler/analyzer.c tests/test_analyzer.c
	$(CC) $(CFLAGS) -o $@ $^

test_codegen: lexer/lexer.c ast/ast.c parser/parser.c compiler/analyzer.c compiler/codegen.c tests/test_codegen.c
	$(CC) $(CFLAGS) -o $@ $^

test_runtime: runtime/runtime.c tests/test_runtime.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: test_lexer test_parser test_analyzer test_codegen test_runtime
	@echo ""
	@echo "── Lexer ──────────────────────────────"
	@./test_lexer   | tail -3
	@echo ""
	@echo "── Parser ─────────────────────────────"
	@./test_parser  | tail -4
	@echo ""
	@echo "── Analyzer ───────────────────────────"
	@./test_analyzer
	@echo ""
	@echo "── Codegen ────────────────────────────"
	@./test_codegen | grep "Codegen errors"
	@echo ""
	@echo "── Runtime ────────────────────────────"
	@./test_runtime
	@echo ""
	@echo "✅  All tests done"

run: venom
	./venom $(FILE) --verbose

# Link a Taipan program with the runtime:  make link FILE=examples/hello.tp
link: runtime venom
	./venom $(FILE)
	clang -o $(basename $(FILE)) $(basename $(FILE)).ll runtime/libtaipan.a -lm
	@echo "✅  $(basename $(FILE)) linked with runtime"

clean:
	rm -f venom $(OBJS)
	rm -f runtime/runtime.o runtime/libtaipan.a
	rm -f test_lexer test_parser test_analyzer test_codegen test_runtime
	rm -f examples/*.ll examples/*.s
	@echo "🧹  Cleaned"

# ── VPK package manager ──────────────────────
vpk_bin: vpk/vpk.o vpk/main.o
	$(CC) $(CFLAGS) -o vpk_bin $^ $(LDFLAGS)
	@mv vpk_bin vpk_tool
	@echo "✅  vpk built"

vpk/vpk.o: vpk/vpk.c vpk/vpk.h
	$(CC) $(CFLAGS) -Ivpk -c $< -o $@

vpk/main.o: vpk/main.c vpk/vpk.h
	$(CC) $(CFLAGS) -Ivpk -c $< -o $@