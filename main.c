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


//	scanner
int			character;
int			lexeme;

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
	while(isspace(character)) {
		if(character == '\n' && no_whitespace) {
			no_whitespace = 0;
			return ';';
		}
		read_char();
	}

	// ignore comment
	if(character == '/') {
		if(read_char() != '/') return '/';
		while(character != '\n') read_char();
		if(no_whitespace) {
			no_whitespace = 0;
			return ';';
		}
		while(isspace(read_char())) {}
	}

	// one character token
	if(strchr("-+*%&|^~!=<>;:()", character)) {
		no_whitespace = 1;
		return read_char();
	}

	// TODO: char

	// string
	if(character == '"') {
		no_whitespace = 1;
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
		no_whitespace = 1;
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
		no_whitespace = 1;
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
	if(l < LEX_SIZE) printf("%s ", keywords[l]);
	else printf("%c ", l);


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






int		frame;
int		param;
char	func_name[1024];

void param_decl() {


}

void minilang() {
	fprintf(dst_file, "\t.intel_syntax noprefix\n");
	fprintf(dst_file, "\t.text\n");

	while(lexeme != LEX_EOF) {
		expect(LEX_IDENT);

		// only functions for now
		strcpy(func_name, token);
		expect('(');
		frame = 8;
		param = 0;
		if(lexeme == LEX_IDENT) {
			param_decl();
			while(read_lexeme() == ',') param_decl();
		}
		expect(')');
		// function body here...
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

