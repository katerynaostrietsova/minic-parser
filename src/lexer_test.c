#include <stdio.h>
#include "parser.tab.h"

extern FILE *yyin;
extern int   yylineno;
extern char *yytext;
extern int   lex_errors;
int yylex(void);

YYSTYPE yylval;
YYLTYPE yylloc;

static const char *token_name(int t)
{
    switch (t) {
        case INT:        return "INT";
        case DOUBLE:     return "DOUBLE";
        case VOID:       return "VOID";
        case IF:         return "IF";
        case ELSE:       return "ELSE";
        case WHILE:      return "WHILE";
        case FOR:        return "FOR";
        case RETURN:     return "RETURN";
        case BREAK:      return "BREAK";
        case CONTINUE:   return "CONTINUE";
        case ID:         return "ID";
        case STRING:     return "STRING";
        case INT_LIT:    return "INT_LIT";
        case FLOAT_LIT:  return "FLOAT_LIT";
        case ADD_ASSIGN: return "ADD_ASSIGN";
        case SUB_ASSIGN: return "SUB_ASSIGN";
        case MUL_ASSIGN: return "MUL_ASSIGN";
        case DIV_ASSIGN: return "DIV_ASSIGN";
        case MOD_ASSIGN: return "MOD_ASSIGN";
        case OR:         return "OR";
        case AND:        return "AND";
        case EQ:         return "EQ";
        case NE:         return "NE";
        case LE:         return "LE";
        case GE:         return "GE";
        case INC:        return "INC";
        case DEC:        return "DEC";
        default:         return NULL;
    }
}

static void print_escaped(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')      printf("\\n");
        else if (*s == '\t') printf("\\t");
        else                 putchar(*s);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) { perror(argv[1]); return 1; }
    }

    printf("%-5s %-11s %s\n", "LINE", "TOKEN", "LEXEME / VALUE");
    printf("----- ----------- ------------------------------\n");

    int t, count = 0;
    while ((t = yylex()) != 0) {
        const char *name = token_name(t);
        char buf[8];
        if (!name) { snprintf(buf, sizeof buf, "'%c'", t); name = buf; }

        printf("%-5d %-11s ", yylineno, name);
        switch (t) {
            case ID:        printf("%s", yylval.sval); break;
            case INT_LIT:   printf("%d", yylval.ival); break;
            case FLOAT_LIT: printf("%g", yylval.dval); break;
            case STRING:    putchar('"'); print_escaped(yylval.sval); putchar('"'); break;
            default:        printf("%s", yytext); break;
        }
        putchar('\n');
        count++;
    }

    printf("----- ----------- ------------------------------\n");
    printf("tokens: %d, lexical errors: %d\n", count, lex_errors);
    return lex_errors ? 1 : 0;
}
