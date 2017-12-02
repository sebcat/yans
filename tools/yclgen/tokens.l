%option noyywrap noinput nounput

%{

#include <stdlib.h>
#include <string.h>

#define YY_DECL int yylex()

#include "parser.h"

%}

%%

#[^\n]*\n             {}
[ \r\n\t]             {}

[{};]                 {return yytext[0];}
data\[\]              {return DATAARR;}
str\[\]               {return STRARR;}
long\[\]              {return LONGARR;}
data                  {return DATA;}
str                   {return STR;}
long                  {return LONG;}

[a-zA-Z][a-zA-Z0-9_]{0,30} {
    snprintf(yylval.lit, sizeof(yylval.lit), "%s", yytext);
    return LIT;
}

%%
