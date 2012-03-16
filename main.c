#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>


enum {

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
	LEX_EOF,

	LEX_SIZE
};

const char* keywords[] = { "if", "else", "elif", "end", "while", "break", "return", NULL, NULL, NULL, NULL, "identifier", "EOF" };


//	scanner
int			look_char;
int			look_lexeme;

char		token[1024];
long long	number;

int			no_whitespace;
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


int read_char() {
	int c = look_char;
	look_char = fgetc(src_file);
	cursor_pos++;
	if(look_char == '\n') {
		line_number++;
		cursor_pos = 1;
	}
	return c;
}


int scan() {

	// skip whitespace
	while(isspace(look_char)) {
		if(look_char == '\n' && no_whitespace) {
			no_whitespace = 0;
			return ';';
		}
		read_char();
	}

	// ignore comment
	if(look_char == '/') {
		if(read_char() != '/') return '/';
		while(look_char != '\n') read_char();
		if(no_whitespace) {
			no_whitespace = 0;
			return ';';
		}
		while(isspace(read_char())) {}
	}

	// one character token
	if(strchr("-+*%&|^~!=<>;:()", look_char)) {
		no_whitespace = 1;
		return read_char();
	}

	// TODO: char

	// string
	if(look_char == '"') {
		no_whitespace = 1;
		int i = 0;
		do {
			if(look_char == '\\') token[i++] = read_char();
			token[i++] = read_char();
			if(i > 1020) error("string too long");
		} while(look_char != '"');
		token[i++] = read_char();
		token[i] = '\0';
		return LEX_STRING;
	}

	// number
	if(isdigit(look_char)) {
		no_whitespace = 1;
		int i = 0;
		do {
			token[i++] = read_char();
			if(i > 20) error("number too long");
		} while(isdigit(look_char));
		token[i] = '\0';
		number = atoll(token);
		return LEX_NUMBER;
	}

	if(isalpha(look_char) || look_char == '_') {
		no_whitespace = 1;
		int i = 0;
		do {
			token[i++] = read_char();
			if(i > 30) error("ident too long");
		} while(isalnum(look_char) || look_char == '_');
		token[i] = '\0';
		// check for keywords
		for(i = 0; i < LEX_KEYWORD_COUNT; i++) {
			if(strcmp(token, keywords[i]) == 0) return i;
		}
		return LEX_IDENT;
	}

	if(look_char != EOF) error("unknown character");
	return LEX_EOF;
}


void read_lexeme() {
	look_lexeme = scan();
}


void init_scanner() {
	line_number = 1;
	cursor_pos = 1;
	read_char();
	read_lexeme();
}


void expect(int lexeme) {
	if(look_lexeme != lexeme) {
		if(lexeme < LEX_SIZE) error("%s expected", keywords[lexeme]);
		else error("%c expected", lexeme);
	}
	read_lexeme();
}


void minilang() {
	fprintf(dst_file, "\t.intel_syntax noprefix\n");
	fprintf(dst_file, "\t.text\n");

	while(look_lexeme == LEX_IDENT) {
		read_lexeme();

	}
	expect(LEX_EOF);
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

