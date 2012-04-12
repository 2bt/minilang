
all:
	c99 main.c -o minilang -g -Wall


test:
	./minilang test.mini test.s
	gcc test.s
	./a.out
