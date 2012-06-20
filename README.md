# minilang

Inspired by [BASICO](http://www.andreadrian.de/tbng/index.html), minilang is a little programming language.
It is bootstrapped, thus proving to be reasonably expressive. The compiler outputs x86-64 ASM code.
Similar to Python, code blocks are formed via indentation. There is no type checking whatsoever. So be careful.
Please, look into `minilang.mini` and you will quickly get the idea.


Compile the throwaway compiler:

	$ c99 minilang.c -o minilang

Bootstrap thusly:

	$ ./minilang minilang.mini minilang.s		# compile with throwaway compiler
	gcc minilang.s -o minilang_jr			# GCC assembles the executable
	./minilang_jr minilang.mini minilang_jr.s	# compile with bootstrapped compiler
	diff minilang.s minilang_jr.s			# compare output -> equal

