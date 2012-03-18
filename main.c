#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>


enum {
	LEX_EOF = EOF,
	LEX_IF = 0,
	LEX_ELSE,
	LEX_ELIF,
	LEX_END,
	LEX_WHILE,
	LEX_BREAK,
	LEX_RETURN,
	LEX_KEYWORD_COUNT,

	LEX_CHAR,
	LEX_STRING,
	LEX_NUMBER,
	LEX_IDENT,

	LEX_SIZE
};

const char* keywords[] = { "if", "else", "elif", "end", "while", "break",
	"return", NULL, NULL, NULL, "number", "identifier" };
const char* call_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };


//	scanner
int			character;
int			lexeme;

char		token[1024];
long long	number;

int			line_number;
int			cursor_pos;

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

	// skip whitespace
	while(isspace(character)) read_char();

	// ignore comment
	if(character == '/') {
		read_char();
		if(character != '/') return '/';
		while(read_char() != '\n') {}
		while(isspace(character)) read_char();
	}

	// one character token
	if(strchr("-+*%&|^~!=<>;:(),", character)) {
		return read_char();
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

	if(isalpha(character) || character == '_') {
		int i = 0;
		do {
			token[i++] = read_char();
			if(i > 30) error("ident too long");
		} while(isalnum(character) || character == '_');
		token[i] = '\0';

		// check for keywords
		for(i = 0; i < LEX_KEYWORD_COUNT; i++) {
			if(strcmp(token, keywords[i]) == 0) return i;
		}
		return LEX_IDENT;
	}

	if(character != EOF) error("unknown character");
	return LEX_EOF;
}


int read_lexeme() {
	int l = lexeme;

//	if(l < LEX_SIZE) printf("%s ", keywords[l]);
//	else printf("%c ", l);

	lexeme = scan();
	return l;
}


void init_scanner() {
	line_number = 1;
	read_char();
	read_lexeme();
}


void expect(int l) {
	if(lexeme != l) {
		if(l < LEX_SIZE) error("%s expected", keywords[l]);
		else error("%c expected", l);
	}
	read_lexeme();
}


struct Local {
	char	name[1024];
	int		offset;

};


int				local_count;
struct Local	locals[1024];
int				frame;
int				param_count;


void add_local(char* name, int offset) {
	for(int i = 0; i < 1024; i++) {
		if(i == local_count) {
			strcpy(name, locals[i].name);
			locals[i].offset = offset;
			local_count++;
			return;
		}
		if(strcmp(name, locals[i].name) == 0) error("multiple declaration");
	}
	error("too many locals");
}


void param_decl() {
	expect(LEX_IDENT);
	frame += 8;
	param_count++;

	add_local(token, frame);
}


void minilang() {
	output("\t.intel_syntax noprefix\n");
	output("\t.text\n");

	while(lexeme != LEX_EOF) {
		expect(LEX_IDENT);


		// only functions for now
		output("\t.globl %s\n", token);
		output("%s:\n", token);
		output("\tpush rbp\n");
		output("\tmov rbp, rsp\n");

		frame = 0;
		param_count = 0;
		local_count = 0;

		expect('(');
		if(lexeme == LEX_IDENT) {
			param_decl();
			while(lexeme == ',') {
				read_lexeme();
				param_decl();
			}
		}
		expect(')');

		if(frame > 0) output("\tsub rsp, %d\n", frame);
		for(int i = 0; i < param_count; i++) {
			output("\tmov [rbp - %d], %s\n", i * 8 + 8, call_regs[i]);
		}

		// function body here...



		expect(LEX_END);
		output("\tleave\n");
		output("\tret\n");
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


	init_scanner();
	minilang();


	return 0;
}

