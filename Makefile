
all:
	c99 main.c -o minilang -g -Wall


boot:
	./minilang bootstrap.mini b.s
	gcc b.s -o b
	./b test.mini

