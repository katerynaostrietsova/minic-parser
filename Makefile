CC     = gcc
CFLAGS = -Wall -g -Isrc

GEN    = src/parser.tab.c src/parser.tab.h src/lex.yy.c
SRCS   = src/main.c src/ast.c src/semantic.c src/interp.c src/codegen_py.c src/ast_json.c
HDRS   = src/ast.h src/semantic.h src/interp.h src/codegen_py.h src/ast_json.h

all: minic

src/parser.tab.c src/parser.tab.h: src/parser.y
	bison -d -v --report=state,solved -o src/parser.tab.c src/parser.y

src/lex.yy.c: src/lexer.l src/parser.tab.h
	flex -o src/lex.yy.c src/lexer.l

minic: $(GEN) $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o minic src/parser.tab.c src/lex.yy.c $(SRCS)

lexer_test: src/lex.yy.c src/lexer_test.c src/parser.tab.h src/ast.h
	$(CC) $(CFLAGS) -o lexer_test src/lex.yy.c src/lexer_test.c

test-parse: minic
	@for f in examples/*.c; do ./minic parse $$f || exit 1; done

conflicts:
	@for g in conflicts/*.y; do \
		bison -v -o /tmp/conflict.tab.c $$g 2>&1; \
		cp /tmp/conflict.output $${g%.y}.output; \
	done

clean:
	rm -f $(GEN) src/parser.output minic lexer_test conflicts/*.output
	rm -rf out

ast: minic
	@for f in examples/*.c; do echo "$$f"; ./minic ast $$f || exit 1; done

test-run: minic
	@mkdir -p out
	@pass=0; fail=0; \
	for f in examples/*.c; do \
		n=$$(basename $$f .c); \
		gcc -w -o out/$$n.gcc $$f && ./out/$$n.gcc > out/$$n.expected; \
		./minic run $$f > out/$$n.interp; \
		if diff -q out/$$n.expected out/$$n.interp > /dev/null; then \
			echo "PASS  $$f"; pass=$$((pass+1)); \
		else \
			echo "FAIL  $$f"; diff out/$$n.expected out/$$n.interp | head -5; fail=$$((fail+1)); \
		fi; \
	done; \
	echo "interpreter vs gcc: $$pass passed, $$fail failed"; [ $$fail -eq 0 ]

test-py: minic
	@mkdir -p out
	@pass=0; fail=0; \
	for f in examples/*.c; do \
		n=$$(basename $$f .c); \
		gcc -w -o out/$$n.gcc $$f && ./out/$$n.gcc > out/$$n.expected; \
		./minic py $$f > out/$$n.py && python3 out/$$n.py > out/$$n.pyout; \
		if diff -q out/$$n.expected out/$$n.pyout > /dev/null; then \
			echo "PASS  $$f -> out/$$n.py"; pass=$$((pass+1)); \
		else \
			echo "FAIL  $$f -> out/$$n.py"; diff out/$$n.expected out/$$n.pyout | head -5; fail=$$((fail+1)); \
		fi; \
	done; \
	echo "python vs gcc: $$pass passed, $$fail failed"; [ $$fail -eq 0 ]

test: test-parse test-run test-py

web: minic
	@mkdir -p web/data
	@names=""; \
	for f in examples/*.c; do \
		n=$$(basename $$f .c); \
		./minic json $$f > web/data/$$n.json || exit 1; \
		names="$$names\"$$n\","; \
	done; \
	echo "[$${names%,}]" > web/data/index.json

serve: web
	@python3 -m http.server 8000 --directory web

.PHONY: all clean test-parse conflicts ast test-run test-py test web serve
