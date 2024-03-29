%option noyywrap noinput nounput
%option reentrant
%option bison-bridge

%{

#include <stdlib.h>
#include <string.h>

#include "yclgen.h"
#include "parser.h"

%}

%%

#[^\n]*\n             {}
[ \r\n\t]             {}

[{};]                 {return yytext[0];}
data\[\]              {return DATAARR;}
long\[\]              {return LONGARR;}
data                  {return DATA;}
long                  {return LONG;}

[a-zA-Z][a-zA-Z0-9_]{0,30} {
    snprintf(yylval->lit, sizeof(yylval->lit), "%s", yytext);
    return LIT;
}

%%
