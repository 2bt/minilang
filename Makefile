
all:
	c99 minilang.c -o minilang -g -Wall


bootstrap: minilang
	./minilang minilang.mini minilang.s
	gcc minilang.s -o minilang_jr
	./minilang_jr minilang.mini minilang_jr.s
	diff minilang.s minilang_jr.s
