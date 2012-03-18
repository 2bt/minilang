
all:
	c99 *.c -o minilang -g -Wall


test:
	./minilang test.mini s.s
	gcc s.s
	./a.out
