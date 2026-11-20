%token ID
%%
stmt : decl ';'
     | expr ';'
     ;
decl : type '*' ID ;
expr : var '*' var
     | var
     ;
type : ID ;
var  : ID ;
