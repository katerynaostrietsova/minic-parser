%code requires {
#include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>

int  yylex(void);
void yyerror(const char *msg);

extern int yylineno;
int parse_errors = 0;
%}

%define parse.error detailed
%define parse.trace
%locations
%expect 0

%union {
    int    ival;
    double dval;
    char  *sval;
    Node  *node;
}

%token INT "int" DOUBLE "double" VOID "void"
%token IF "if" ELSE "else" WHILE "while" FOR "for"
%token RETURN "return" BREAK "break" CONTINUE "continue"

%token <sval> ID        "identifier"
%token <sval> STRING    "string literal"
%token <ival> INT_LIT   "integer literal"
%token <dval> FLOAT_LIT "floating literal"

%token ADD_ASSIGN "+=" SUB_ASSIGN "-=" MUL_ASSIGN "*=" DIV_ASSIGN "/=" MOD_ASSIGN "%="
%token OR "||" AND "&&" EQ "==" NE "!=" LE "<=" GE ">=" INC "++" DEC "--"

%type <ival> type
%type <node> program external_list external func_def params_opt param_list param
%type <node> var_decl init_list init block item_list_opt item_list item
%type <node> stmt for_init expr_opt expr args_opt arg_list

%printer { fprintf(yyo, "%s", $$); } <sval>
%printer { fprintf(yyo, "%d", $$); } <ival>
%printer { fprintf(yyo, "%g", $$); } <dval>
%printer { fprintf(yyo, "%s", $$ ? kind_name($$->kind) : "-"); } <node>

%destructor { free($$); }     <sval>
%destructor { ast_free($$); } <node>

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%left  OR
%left  AND
%left  EQ NE
%left  '<' '>' LE GE
%left  '+' '-'
%left  '*' '/' '%'
%right '!' UMINUS

%start program

%%

program
    : external_list                     { ast_root = $1; $$ = NULL; }
    ;

external_list
    : external                          { $$ = node_add(node_new(N_PROGRAM, @1.first_line), $1); }
    | external_list external            { $$ = node_add($1, $2); }
    ;

external
    : func_def
    | var_decl
    ;

func_def
    : type ID '(' params_opt ')' block
        {
            $$ = node_new(N_FUNC, @2.first_line);
            $$->type = $1;
            $$->name = $2;
            node_add($$, $4);
            node_add($$, $6);
        }
    ;

type
    : INT                               { $$ = T_INT; }
    | DOUBLE                            { $$ = T_DOUBLE; }
    | VOID                              { $$ = T_VOID; }
    ;

params_opt
    : %empty                            { $$ = node_new(N_PARAMS, yylineno); }
    | param_list
    ;

param_list
    : param                             { $$ = node_add(node_new(N_PARAMS, @1.first_line), $1); }
    | param_list ',' param              { $$ = node_add($1, $3); }
    ;

param
    : type ID
        {
            $$ = node_new(N_PARAM, @2.first_line);
            $$->type = $1;
            $$->name = $2;
        }
    ;

var_decl
    : type init_list ';'                { $$ = $2; $$->type = $1; }
    ;

init_list
    : init                              { $$ = node_add(node_new(N_VAR_DECL, @1.first_line), $1); }
    | init_list ',' init                { $$ = node_add($1, $3); }
    ;

init
    : ID                                { $$ = node_new(N_VAR, @1.first_line); $$->name = $1; }
    | ID '=' expr                       { $$ = node_new(N_VAR, @1.first_line); $$->name = $1;
                                          node_add($$, $3); }
    ;

block
    : '{' item_list_opt '}'             { $$ = $2; $$->line = @1.first_line; }
    ;

item_list_opt
    : %empty                            { $$ = node_new(N_BLOCK, yylineno); }
    | item_list
    ;

item_list
    : item                              { $$ = node_add(node_new(N_BLOCK, @1.first_line), $1); }
    | item_list item                    { $$ = node_add($1, $2); }
    ;

item
    : var_decl
    | stmt
    ;

stmt
    : block
    | expr ';'                          { $$ = node_add(node_new(N_EXPR_STMT, @1.first_line), $1); }
    | ';'                               { $$ = node_new(N_EMPTY, @1.first_line); }
    | IF '(' expr ')' stmt              %prec LOWER_THAN_ELSE
        {
            $$ = node_new(N_IF, @1.first_line);
            node_add($$, $3);
            node_add($$, $5);
        }
    | IF '(' expr ')' stmt ELSE stmt
        {
            $$ = node_new(N_IF, @1.first_line);
            node_add($$, $3);
            node_add($$, $5);
            node_add($$, $7);
        }
    | WHILE '(' expr ')' stmt
        {
            $$ = node_new(N_WHILE, @1.first_line);
            node_add($$, $3);
            node_add($$, $5);
        }
    | FOR '(' for_init ';' expr_opt ';' expr_opt ')' stmt
        {
            $$ = node_new(N_FOR, @1.first_line);
            node_add($$, $3);
            node_add($$, $5);
            node_add($$, $7);
            node_add($$, $9);
        }
    | RETURN expr_opt ';'
        {
            $$ = node_new(N_RETURN, @1.first_line);
            if ($2->kind != N_EMPTY) node_add($$, $2);
            else ast_free($2);
        }
    | BREAK ';'                         { $$ = node_new(N_BREAK, @1.first_line); }
    | CONTINUE ';'                      { $$ = node_new(N_CONTINUE, @1.first_line); }
    | error ';'                         { yyerrok; $$ = node_new(N_EMPTY, @2.first_line); }
    ;

for_init
    : expr_opt
    | type init_list                    { $$ = $2; $$->type = $1; }
    ;

expr_opt
    : %empty                            { $$ = node_new(N_EMPTY, yylineno); }
    | expr
    ;

expr
    : ID '=' expr                       { $$ = mk_assign("=",  $1, $3, @1.first_line); }
    | ID ADD_ASSIGN expr                { $$ = mk_assign("+=", $1, $3, @1.first_line); }
    | ID SUB_ASSIGN expr                { $$ = mk_assign("-=", $1, $3, @1.first_line); }
    | ID MUL_ASSIGN expr                { $$ = mk_assign("*=", $1, $3, @1.first_line); }
    | ID DIV_ASSIGN expr                { $$ = mk_assign("/=", $1, $3, @1.first_line); }
    | ID MOD_ASSIGN expr                { $$ = mk_assign("%=", $1, $3, @1.first_line); }
    | expr OR  expr                     { $$ = mk_binop("||", $1, $3, @2.first_line); }
    | expr AND expr                     { $$ = mk_binop("&&", $1, $3, @2.first_line); }
    | expr EQ  expr                     { $$ = mk_binop("==", $1, $3, @2.first_line); }
    | expr NE  expr                     { $$ = mk_binop("!=", $1, $3, @2.first_line); }
    | expr '<' expr                     { $$ = mk_binop("<",  $1, $3, @2.first_line); }
    | expr '>' expr                     { $$ = mk_binop(">",  $1, $3, @2.first_line); }
    | expr LE  expr                     { $$ = mk_binop("<=", $1, $3, @2.first_line); }
    | expr GE  expr                     { $$ = mk_binop(">=", $1, $3, @2.first_line); }
    | expr '+' expr                     { $$ = mk_binop("+",  $1, $3, @2.first_line); }
    | expr '-' expr                     { $$ = mk_binop("-",  $1, $3, @2.first_line); }
    | expr '*' expr                     { $$ = mk_binop("*",  $1, $3, @2.first_line); }
    | expr '/' expr                     { $$ = mk_binop("/",  $1, $3, @2.first_line); }
    | expr '%' expr                     { $$ = mk_binop("%",  $1, $3, @2.first_line); }
    | '-' expr          %prec UMINUS    { $$ = mk_unop("-", $2, @1.first_line); }
    | '!' expr                          { $$ = mk_unop("!", $2, @1.first_line); }
    | INC ID                            { $$ = mk_incdec("++", $2, 1, @1.first_line); }
    | DEC ID                            { $$ = mk_incdec("--", $2, 1, @1.first_line); }
    | ID INC                            { $$ = mk_incdec("++", $1, 0, @1.first_line); }
    | ID DEC                            { $$ = mk_incdec("--", $1, 0, @1.first_line); }
    | ID '(' args_opt ')'               { $$ = $3; $$->name = $1; $$->line = @1.first_line; }
    | '(' expr ')'                      { $$ = $2; }
    | ID                                { $$ = mk_id($1, @1.first_line); }
    | INT_LIT                           { $$ = mk_int($1, @1.first_line); }
    | FLOAT_LIT                         { $$ = mk_float($1, @1.first_line); }
    | STRING                            { $$ = mk_string($1, @1.first_line); }
    ;

args_opt
    : %empty                            { $$ = node_new(N_CALL, yylineno); }
    | arg_list
    ;

arg_list
    : expr                              { $$ = node_add(node_new(N_CALL, @1.first_line), $1); }
    | arg_list ',' expr                 { $$ = node_add($1, $3); }
    ;

%%

void yyerror(const char *msg)
{
    fprintf(stderr, "line %d: %s\n", yylineno, msg);
    parse_errors++;
}
