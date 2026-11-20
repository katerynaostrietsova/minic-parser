%token IF ELSE COND S
%%
stmt : IF '(' COND ')' stmt
     | IF '(' COND ')' stmt ELSE stmt
     | S ';'
     ;
