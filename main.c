#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


enum {
	LEX_EOF = EOF,
	LEX_ASM = 0,
	LEX_IF,
	LEX_ELSE,
	LEX_ELIF,
	LEX_WHILE,
	LEX_BREAK,
	LEX_CONTINUE,
	LEX_RETURN,
	LEX_VAR,
	LEX_KEYWORD_COUNT,
	LEX_BLOCK_END,
	LEX_CHAR,
	LEX_STRING,
	LEX_NUMBER,
	LEX_IDENT,
	LEX_ASM_LINE,
	LEX_LE,
	LEX_GE,
	LEX_EQ,
	LEX_NE,
	LEX_SIZE
};

const char* keywords[] = {
	"asm",
	"if",
	"else",
	"elif",
	"while",
	"break",
	"continue",
	"return",
	"var",
	NULL,
	"block end",
	"character",
	"string",
	"number",
	"identifier",
};


//	scanner
int			character;
int			lexeme;
char		token[1024];
long long	number;
int			neg_number;
int			line_number = 0;
int			cursor_pos = 0;

int			brackets = 0;
int			block = 0;
int			indent = 0;
int			newline = 1;
int			asm_active = 0;

FILE*		src_file;
FILE*		dst_file;


void error(char* msg, ...) {
	fprintf(stderr, "%d:%d: error: ", line_number, cursor_pos);
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}


void output(char* msg, ...) {
	va_list args;
	va_start(args, msg);
	vfprintf(dst_file, msg, args);
	va_end(args);
}


int read_char() {
	int c = character;
	character = fgetc(src_file);
	cursor_pos++;
	if(character == '\n') {
		line_number++;
		cursor_pos = 0;
	}
	return c;
}


int scan() {
space:
	while(isspace(character)) {
		if(newline) {
			if(character == ' ') indent++;
			if(character == '\t') indent = (indent & ~3) + 4;
		}
		if(character == '\n') {
			indent = 0;
			int n = newline;
			newline = 1;
			if(n == 0 && brackets == 0) return ';';
		}
		read_char();
	}

	// ignore comment
	if(character == '#') {
		while(character != '\n') read_char();
		goto space;
	}

	// indent
	if(indent > block) error("invalid indentation");
	if(indent < block) {
		asm_active = 0;
		block -= 4;
		return LEX_BLOCK_END;
	}

	// asm line
	if(asm_active) {
		int i = 0;
		while(character != '\n') {
			token[i] = read_char();
			i++;
		}
		token[i] = '\0';
		return LEX_ASM_LINE;
	}

	newline = 0;
	// one character token
	if(strchr("-+*/%&|~!=<>;:()[],@{}", character)) {
		int c = character;
		read_char();
		if(c == ':') {	// new block
			block += 4;
			indent += 4;
		}
		else if(strchr("<>!=", c) && character == '=') {
			read_char();
			switch(c) {
			case '<': return LEX_LE;
			case '>': return LEX_GE;
			case '=': return LEX_EQ;
			case '!': return LEX_NE;
			}
		}
		else if(c == '(' || c == '[') brackets++;
		else if(c == ')' || c == ']') brackets--;
		if(isdigit(character)) neg_number = (c == '-');
		return c;
	}

	// TODO: char

	// string
	if(character == '"') {
		int i = 0;
		do {
			if(character == '\\') token[i++] = read_char();
			token[i++] = read_char();
			if(i > 1020) error("string too long");
		} while(character != '"');
		token[i++] = read_char();
		token[i] = '\0';
		return LEX_STRING;
	}

	// number
	if(isdigit(character)) {
		int i = 0;
		do {
			token[i++] = read_char();
			if(i > 20) error("number too long");
		} while(isdigit(character));
		token[i] = '\0';
		number = atoll(token);
		return LEX_NUMBER;
	}

	// identifier and keywords
	if(isalpha(character) || character == '_') {
		int i = 0;
		do {
			token[i++] = read_char();
			if(i > 62) error("identifier too long");
		} while(isalnum(character) || character == '_');
		token[i] = '\0';

		// check for keywords
		for(i = 0; i < LEX_KEYWORD_COUNT; i++) {
			if(strcmp(token, keywords[i]) == 0) return i;
		}
		return LEX_IDENT;
	}

	if(character != EOF) error("unknown character");
	if(block > 0) {
		block -= 4;
		puts("ND");
		return LEX_BLOCK_END;
	}
	return LEX_EOF;
}


void read_lexeme() { lexeme = scan(); }


void init_scanner() {
	line_number = 1;
	read_char();
	read_lexeme();
}


void expect(int l) {
	if(lexeme != l) {
		if(l < LEX_SIZE) error("%s expected", keywords[l]);
		else error("<%c> expected", l);
	}
	read_lexeme();
}


// symbol table
typedef struct {
	char	name[64];
	int		offset;
} Variable;

Variable	locals[1024];
int			local_count;


// code generation
const char* call_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
const char*	regs[] = { "r8", "r9", "r11", "rax" };
enum {
			cache_size = sizeof(regs) / sizeof(char*)
};
int			cache[cache_size];
int			stack_size;
int			label = 0;
int			while_labels[256];
int			while_level = -1;


const char* regname(int i) { return regs[cache[i]]; }


void init_cache() {
	for(int i = 0; i < cache_size; i++) cache[i] = i;
	stack_size = 0;
}


Variable* lookup_variable(char* name, Variable* table, int size) {
	for(int i = 0; i < size; i++) {
		if(strcmp(name, table[i].name) == 0) return &table[i];
	}
	return NULL;
}


Variable* lookup_local(char* name) {
	return lookup_variable(name, locals, local_count);
}


void add_variable(char* name, int offset, Variable* table, int size) {
	for(int i = 0; i < 1024; i++) {
		if(i == size) {
			strcpy(table[i].name, name);
			table[i].offset = offset;
			return;
		}
		if(strcmp(name, table[i].name) == 0) error("multiple declarations");
	}
	error("too many variables");
}


void add_local(char* name, int offset) {
	add_variable(name, offset, locals, local_count);
	local_count++;
}


void push() {
	int i = cache_size - 1;
	int tmp = cache[i];
	if(stack_size >= cache_size) output("\tpush %s\n", regs[tmp]);
	while(i > 0) {
		cache[i] = cache[i - 1];
		i--;
	}
	cache[0] = tmp;
	stack_size++;
}


void pop() {
	stack_size--;
	if(stack_size == 0) init_cache();
	else {
		int i = 0;
		int tmp = cache[0];
		while(i < cache_size - 1) {
			cache[i] = cache[i + 1];
			i++;
		}
		cache[i] = tmp;
		if(stack_size >= cache_size) output("\tpop %s\n", regs[i]);
	}
}


int is_expr_beginning() {
	static const int lexemes[] = {
		'-', '!', '(', LEX_NUMBER, LEX_STRING, LEX_IDENT
	};
	for(int i = 0; i < sizeof(lexemes) / sizeof(int); i++)
		if(lexeme == lexemes[i]) return 1;
	return 0;
}


int is_stmt_beginning() {
	static const int lexemes[] = {
		LEX_ASM, LEX_IF, LEX_WHILE, LEX_BREAK, LEX_CONTINUE, LEX_RETURN, ';'
	};
	for(int i = 0; i < sizeof(lexemes) / sizeof(int); i++)
		if(lexeme == lexemes[i]) return 1;
	return is_expr_beginning();
}


void expression();
void expr_level_zero() {

	if(lexeme == '!') {
		read_lexeme();
		expr_level_zero();
		output("\ttest %s, %s\n", regname(0), regname(0));
		output("\tsetz cl\n");
		output("\tmovzx %s, cl\n", regname(0));
		return;
	}
	if(lexeme == '-') {
		if(!neg_number) {
			read_lexeme();
			expr_level_zero();
			output("\tneg %s\n", regname(0));
			return;
		}
		read_lexeme();
		push();
		output("\tmov %s, %ld\n", regname(0), -number);
		expr_level_zero(LEX_NUMBER);
	}
	else if(lexeme == LEX_NUMBER) {
		push();
		output("\tmov %s, %ld\n", regname(0), number);
		read_lexeme();
	}
	else if(lexeme == '(') {
		read_lexeme();
		expression();
		expect(')');
	}
	else if(lexeme == LEX_IDENT) {
		Variable* v = lookup_local(token);

		read_lexeme();
		if(lexeme == '(') {	// function call
			char name[64];
			strcpy(name, token);

			// TODO: save and restore cache
/*			// push currently used registers
			int used_regs = (stack_size > cache_size) ? cache_size : stack_size;
			for(int i = used_regs - 1; i >= 0; i--)
				output("\tpush %s\n", regs[cache[i]]);
*/
//			for(int i = 0; i < cache_size; i++) push();


			int old_size = stack_size;
			stack_size = 0;

			// expr list
			int args = 0;
			read_lexeme();
			if(is_expr_beginning()) {
				args++;
				expression();
				output("\tpush %s\n", regname(0));
				pop();
				while(lexeme == ',') {
					read_lexeme();
					args++;
					if(args > 6) error("too many arguments");
					expression();
					output("\tpush %s\n", regname(0));
					pop();
				}
			}
			expect(')');

			// just checking...
			assert(stack_size == 0);

			// set up registers
			for(int i = args - 1; i >= 0; i--) {
				output("\tpop %s\n", call_regs[i]);
			}
			output("\txor rax, rax\n");

			// call
			output("\tcall %s\n", name);

			// return value in rax
			init_cache();
			push();
			stack_size = old_size + 1;

		}
		else if(lexeme == '=') {
			if(!v) error("variable not found");
			read_lexeme();
			expression();
			output("\tmov QWORD PTR [rbp - %d], %s\n", v->offset, regname(0));
		}
		else {
			if(!v) error("variable not found");
			push();
			output("\tmov %s, QWORD PTR [rbp - %d]\n", regname(0), v->offset);
		}
	}
	else if(lexeme == '@') {
		// dereference
		error("not implementet yet");
	}
	else if(lexeme == LEX_STRING) {
		push();
		output("\t.section .rodata\n");
		output("LC%d:\n", label);
		output("\t.string %s\n", token);
		output("\t.text\n");
		output("\tmov %s, OFFSET LC%d\n", regname(0), label);
		label++;
		read_lexeme();
	}
	else error("bad expression");

	while(lexeme == '[') {
		read_lexeme();
		expression();
		expect(']');
		if(lexeme == '=') {
			read_lexeme();
			expression();
			output("\tmov QWORD PTR [%s + %s * 8], %s\n", regname(2), regname(1), regname(0));
			int tmp = cache[2];
			cache[2] = cache[0];
			cache[0] = tmp;
			pop();
			pop();
			return;
		}
		output("\tmov %s, QWORD PTR [%s + %s * 8]\n", regname(1), regname(1), regname(0));
		pop();
	}
	if(lexeme == '{') {
		read_lexeme();
		expression();
		expect('}');
		if(lexeme == '=') {
			read_lexeme();
			expression();
			output("\tmov rcx, %s\n", regname(0));
			output("\tmov BYTE PTR [%s + %s], cl\n", regname(2), regname(1));
			int tmp = cache[2];
			cache[2] = cache[0];
			cache[0] = tmp;
			pop();
			pop();
			return;
		}
		output("\tmov cl, BYTE PTR [%s + %s]\n", regname(1), regname(0));
		output("\tmovzx %s, cl\n", regname(1));
		pop();
	}

}


void expr_level_one() {
	expr_level_zero();
	while(strchr("*%/", lexeme)) {
		if(lexeme == '*') {
			read_lexeme();
			expr_level_zero();
			output("\timul %s, %s\n", regname(1), regname(0));
			pop();
		}
		else if(lexeme == '%') {
			error("TODO");
		}
		else if(lexeme == '/') {
			error("TODO");
		}
	}
}


void expr_level_two() {
	expr_level_one();
	while(strchr("+-", lexeme)) {
		if(lexeme == '+') {
			read_lexeme();
			expr_level_one();
			output("\tadd %s, %s\n", regname(1), regname(0));
			pop();
		}
		else if(lexeme == '-') {
			read_lexeme();
			expr_level_one();
			output("\tsub %s, %s\n", regname(1), regname(0));
			pop();
		}
	}
}


void expr_level_three() {
	expr_level_two();
	char* comp;
	switch(lexeme) {
	case '<': comp = "l"; break;
	case '>': comp = "g"; break;
	case LEX_LE: comp = "le"; break;
	case LEX_GE: comp = "ge"; break;
	case LEX_EQ: comp = "e"; break;
	case LEX_NE: comp = "ne"; break;
	default: return;
	}
	read_lexeme();
	expr_level_two();
	output("\tcmp %s, %s\n", regname(1), regname(0));
	output("\tset%s cl\n", comp);
	output("\tmovzx %s, cl\n", regname(1));
	pop();
}


void expr_level_four() {
	expr_level_three();
	while(lexeme == '&') {
		read_lexeme();
		expr_level_three();
		output("\tand %s, %s\n", regname(1), regname(0));
		pop();
	}
}


void expression() {
	expr_level_four();
	while(lexeme == '|') {
		read_lexeme();
		expr_level_four();
		output("\tor %s, %s\n", regname(1), regname(0));
		pop();
	}
}


void statement();
void statement_list() {
	while(is_stmt_beginning()) statement();
}


void statement() {
	if(lexeme == LEX_ASM) {
		read_lexeme();
		asm_active = 1;
		newline = 1;
		expect(':');
		while(lexeme == LEX_ASM_LINE) {
			if(lexeme == LEX_ASM_LINE) output("\t%s\n", token);
			read_lexeme();
		}
		expect(LEX_BLOCK_END);
	}
	else if(lexeme == LEX_IF) {
		read_lexeme();
		expression();
		expect(':');
		int l_end = label++;
		int l_next = label++;
		int end = 0;
		output("\ttest %s, %s\n", regname(0), regname(0));
		output("\tjz .L%d\n", l_next);
		init_cache();
		statement_list();
		expect(LEX_BLOCK_END);
		if(lexeme == LEX_ELIF || lexeme == LEX_ELSE) {
			output("\tjmp .L%d\n", l_end);
			end = 1;
		}
		output(".L%d:\n", l_next);
		while(lexeme == LEX_ELIF) {
			read_lexeme();
			expression();
			expect(':');
			l_next = label++;
			output("\ttest %s, %s\n", regname(0), regname(0));
			output("\tjz .L%d\n", l_next);
			statement_list();
			expect(LEX_BLOCK_END);
			if(lexeme == LEX_ELIF || lexeme == LEX_ELSE)
				output("\tjmp .L%d\n", l_end);
			output(".L%d:\n", l_next);
		}
		if(lexeme == LEX_ELSE) {
			read_lexeme();
			expect(':');
			statement_list();
			expect(LEX_BLOCK_END);
		}
		if(end) output(".L%d:\n", l_end);
	}
	else if(lexeme == LEX_WHILE) {
		read_lexeme();
		while_level++;
		if(while_level == 256) error("while nesting limit exceeded");
		while_labels[while_level] = label;
		label += 2;
		output(".L%d:\n", while_labels[while_level]);
		expression();
		expect(':');
		output("\ttest %s, %s\n", regname(0), regname(0));
		output("\tjz .L%d\n", while_labels[while_level] + 1);
		init_cache();
		statement_list();
		expect(LEX_BLOCK_END);
		output("\tjmp .L%d\n", while_labels[while_level]);
		output(".L%d:\n", while_labels[while_level] + 1);
	}
	else if(lexeme == LEX_BREAK) {
		read_lexeme();
		if(while_level < 0) error("break without while");
		output("\tjmp .L%d\n", while_labels[while_level] + 1);
	}
	else if(lexeme == LEX_CONTINUE) {
		read_lexeme();
		if(while_level < 0) error("continue without while");
		output("\tjmp .L%d\n", while_labels[while_level]);
	}
	else if(lexeme == LEX_RETURN) {
		read_lexeme();
		if(is_expr_beginning()) {
			expression();
			if(strcmp(regname(0), "rax") != 0)
				output("\tmov rax, %s\n", regname(0));
		}
		output("\tleave\n");
		output("\tret\n");
	}
	else if(is_expr_beginning()) {
		expression();
		pop();
	}
	else expect(';');
}


void minilang() {
	init_scanner();

	output("\t.intel_syntax noprefix\n");
	output("\t.text\n");

	while(lexeme != LEX_EOF) {
		expect(LEX_IDENT);


		// only functions in global scope for now
		output("\t.globl %s\n", token);
		output("%s:\n", token);
		output("\tpush rbp\n");
		output("\tmov rbp, rsp\n");

		int frame = 0;
		local_count = 0;

		// parameter list
		int params = 0;
		expect('(');
		if(lexeme == LEX_IDENT) {
			params++;
			expect(LEX_IDENT);
			frame += 8;
			add_local(token, frame);
			while(lexeme == ',') {
				read_lexeme();
				params++;
				if(params > 6) error("too many arguments");
				expect(LEX_IDENT);
				frame += 8;
				add_local(token, frame);
			}
		}
		expect(')');
		expect(':');

		// local variables
		while(lexeme == ';') read_lexeme();
		while(lexeme == LEX_VAR) {
			read_lexeme();
			expect(LEX_IDENT);
			frame += 8;
			add_local(token, frame);
			while(lexeme == ',') {
				read_lexeme();
				expect(LEX_IDENT);
				frame += 8;
				add_local(token, frame);
			}
			while(lexeme == ';') read_lexeme();
		}

		if(frame > 0) output("\tsub rsp, %d\n", frame);
		for(int i = 0; i < params; i++) {
			output("\tmov QWORD PTR [rbp - %d], %s\n", i * 8 + 8, call_regs[i]);
		}

		init_cache();
		statement_list();
		output("\tleave\n");
		output("\tret\n");
		expect(LEX_BLOCK_END);

	}
}


void cleanup() {
	if(src_file) fclose(src_file);
	if(dst_file && dst_file != stdin) fclose(dst_file);
}


int main(int argc, char** argv) {

	if(argc < 2 || argc > 3) {
		printf("usuage: %s <source> [output]\n", argv[0]);
		exit(0);
	}

	src_file = fopen(argv[1], "r");
	if(!src_file) error("opening source file failed");

	if(argc == 3) {
		dst_file = fopen(argv[2], "w");
		if(!src_file) error("opening output file failed");
	}
	else dst_file = stdout;
	atexit(cleanup);
	minilang();
	return 0;
}

