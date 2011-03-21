%{

#include <stdio.h>
#include "util.h"
#include "tokens.h"

extern int yyerror(void*, const char*);
%}

%option noyywrap nounput

WS      [ \t\n]
QTSTR   \"(\\.|[^\\"])*\"
STR     (\\.|\\\"|[^ \t\n<>&"|])+

%%

{WS}    { }


{QTSTR} { 
          yylval.str = str_unescape(str_unquote(strdup(yytext)));
          return STR;
        }

{STR}   { 
          yylval.str = str_unescape(strdup(yytext));
          return STR;
        }

[><|&]  { return yytext[0]; }


.       { yyerror(NULL, "bad character"); }

%%


