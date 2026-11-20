%option noyywrap nounput noinput yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

int lex_errors = 0;

#define YY_USER_ACTION  yylloc.first_line = yylloc.last_line = yylineno;

static char *unescape(const char *src, int len)
{
    char *dst = malloc(len + 1);
    int j = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len) {
            i++;
            switch (src[i]) {
                case 'n':  dst[j++] = '\n'; break;
                case 't':  dst[j++] = '\t'; break;
                case '"':  dst[j++] = '"';  break;
                case '\\': dst[j++] = '\\'; break;
                default:   dst[j++] = '\\'; dst[j++] = src[i]; break;
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
    return dst;
}
%}

DIGIT    [0-9]
LETTER   [A-Za-z_]
EXP      [eE][+-]?{DIGIT}+

%x COMMENT

%%

[ \t\r\n]+              { }
"//".*                  { }
^[ \t]*#.*              { }

"/*"                    { BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>[^*\n]+        { }
<COMMENT>"*"            { }
<COMMENT>\n             { }
<COMMENT><<EOF>>        { fprintf(stderr, "line %d: unterminated comment\n", yylineno);
                          lex_errors++; return 0; }

"int"                   { return INT; }
"double"                { return DOUBLE; }
"void"                  { return VOID; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"for"                   { return FOR; }
"return"                { return RETURN; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }

{LETTER}({LETTER}|{DIGIT})*   { yylval.sval = strdup(yytext); return ID; }

{DIGIT}+"."{DIGIT}*{EXP}?     |
"."{DIGIT}+{EXP}?             |
{DIGIT}+{EXP}                 { yylval.dval = strtod(yytext, NULL); return FLOAT_LIT; }

{DIGIT}+                      { yylval.ival = (int)strtol(yytext, NULL, 10); return INT_LIT; }

\"([^"\\\n]|\\.)*\"           { yylval.sval = unescape(yytext + 1, yyleng - 2); return STRING; }
\"([^"\\\n]|\\.)*             { fprintf(stderr, "line %d: unterminated string\n", yylineno);
                                lex_errors++; }

"+="                    { return ADD_ASSIGN; }
"-="                    { return SUB_ASSIGN; }
"*="                    { return MUL_ASSIGN; }
"/="                    { return DIV_ASSIGN; }
"%="                    { return MOD_ASSIGN; }
"||"                    { return OR; }
"&&"                    { return AND; }
"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
">="                    { return GE; }
"++"                    { return INC; }
"--"                    { return DEC; }

[-+*/%<>=!(){},;]       { return yytext[0]; }

.                       { fprintf(stderr, "line %d: unexpected character '%s'\n",
                                  yylineno, yytext);
                          lex_errors++; }

%%

