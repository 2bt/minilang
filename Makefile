
all:
	c99 main.c -o minilang -g -Wall


test:
	./minilang test.mini s.s
	gcc s.s
	./a.out


boot:
	./minilang bootstrap.mini s.s
	gcc s.s
	./a.out

