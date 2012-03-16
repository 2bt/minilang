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

int		line_number;
int		cursor_pos;

int		no_whitespace;

FILE*	src_file;


void error(char* msg) {
	printf("%d:%d: error: %s\n", line_number, cursor_pos, msg);
	exit(1);
}


void read_char() {
	look_char = fgetc(src_file);
	cursor_pos++;
	if(look_char == '\n') {
		line_number++;
		cursor_pos = 1;
	}
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
		read_char();
		if(look_char != '/') return '/';
		while(look_char != '\n') read_char();
		if(no_whitespace) {
			no_whitespace = 0;
			return ';';
		}
		while(isspace(look_char)) read_char();
	}

	// one character token
	if(strchr("-+*%&|^~!=<>;:()#$@", look_char)) {
		no_whitespace = 1;
		int c = look_char;
		read_char();
		return c;
	}

	// TODO: char

	// string
	if(look_char == '"') {
		no_whitespace = 1;
		int i = 0;
		do {
			if(look_char == '\\') {
				text[i] = look_char;
				i++;
				read_char();
			}
			text[i] = look_char;
			i++;
			read_char();
		} while(look_char != '"');
		text[i] = look_char;
		text[i + 1] = '\0';


		return LEX_STRING;
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
	if(!src_file) error("Could not open source file");

	init_scanner();




	fclose(src_file);

	return 0;
}

