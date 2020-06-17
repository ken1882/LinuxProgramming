%{
#include "main.h"
#include <vector>

int yylex();
int yyerror(char* s);
std::vector<int> CMD_FLAGS;
%}

%token CMD_IDENTIFIER
%token SYM_PIPE RED_STDIN RED_STDOUT APP_STDOUT RUN_DAEMON CONT_INPUT

%%
Commmand
    : CMD_IDENTIFIER 
    | CMD_IDENTIFIER SYM_PIPE Commmand
    | CMD_IDENTIFIER RED_STDIN Commmand
    | CMD_IDENTIFIER RED_STDOUT Commmand
    | CMD_IDENTIFIER APP_STDOUT Commmand
    | CMD_IDENTIFIER RUN_DAEMON
    | CMD_IDENTIFIER CONT_INPUT Commmand
    | CMD_IDENTIFIER Commmand
    ;
%%

int yyerror(char *s){
    printf("Error in command: %s\n", s);
}
