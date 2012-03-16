#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


enum {
	LEX_STRING = 256,
	LEX_NUMBER,
	LEX_IF,
	LEX_ELSE,
	LEX_ELIF,
	LEX_END,

};

//	scanner
int		look_char;
int		look_lexeme;

long long	number;


int		line_number;
int		cursor_pos;

int		no_whitespace;

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
	static char	text[1024];

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
			if(look_char == '\\') text[i++] = read_char();
			text[i++] = read_char();
			if(i > 1020) error("string too long");
		} while(look_char != '"');
		text[i++] = read_char();
		text[i] = '\0';
		return LEX_STRING;
	}

	if(isdigit(look_char)) {
		no_whitespace = 1;
		int i = 0;
		do {
			text[i++] = read_char();
			if(i > 20) error("number too long");
		} while(isdigit(look_char));
		text[i] = '\0';
		puts(text);
		number = atoll(text);
		return LEX_NUMBER;
	}


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

