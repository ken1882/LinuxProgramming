%{
#include "parser.tab.h"

%}

%%
"&"                     {return RUN_DAEMON;}
"|"                     {return SYM_PIPE;}
">"                     {return RED_STDOUT;}
"<"                     {return RED_STDIN;}
">>"                    {return APP_STDOUT;}
"\\"                    {return CONT_INPUT;}
\"(\\.|[^"\\])*\"       {return CMD_IDENTIFIER;}
[ \t\n]                 /* skip blanks */
[~!@#\$%\^&*=\(\)\+\-\[\]\{\}\\\/\'\._a-zA-Z0-9]+     {return CMD_IDENTIFIER;}
.                       {printf("Unexpected character: %s\n", yytext);}
%%

void lex_scan_string(const char* str){
    yy_scan_string(str);
}

void lex_clear_buffer(){
    yy_delete_buffer(YY_CURRENT_BUFFER);
}

int yywrap(){
    return 1;
}
