#include <stdlib.h>
#include <stdio.h>
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

	LEX_CHAR,
	LEX_STRING,
	LEX_NUMBER,
	LEX_IDENT,
};

const char* keywords[] = { "if", "else", "elif", "end", "while", "break", "return", NULL };


//	scanner
int			look_char;
int			look_lexeme;

char		token[1024];
long long	number;

int			no_whitespace;
int			line_number;
int			cursor_pos;

FILE*	src_file;


void error(char* msg) {
	printf("%d:%d: error: %s\n", line_number, cursor_pos, msg);
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

		i = 0;
		while(keywords[i]) {
			if(strcmp(token, keywords[i]) == 0) return i;
			i++;
		}
		return LEX_IDENT;
	}

	if(look_char != LEX_EOF) error("unknown character");
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


int main(int argc, char** argv) {

	if(argc != 2) {
		printf("usuage: %s file\n", argv[0]);
		exit(0);
	}

	src_file = fopen(argv[1], "r");
	if(!src_file) error("opening source file failed");

	init_scanner();



	fclose(src_file);

	return 0;
}

