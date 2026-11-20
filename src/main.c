#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "semantic.h"
#include "interp.h"
#include "codegen_py.h"
#include "ast_json.h"

extern FILE *yyin;
extern int   yydebug;
extern int   lex_errors;
extern int   parse_errors;
int yyparse(void);

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s parse|trace|ast|check|run|py|json <file.c>\n", prog);
}

int main(int argc, char **argv)
{
    if (argc != 3) { usage(argv[0]); return 2; }

    const char *cmd  = argv[1];
    const char *path = argv[2];

    const char *commands[] = { "parse", "trace", "ast", "check", "run", "py", "json", NULL };
    int known = 0;
    for (int i = 0; commands[i]; i++)
        if (strcmp(cmd, commands[i]) == 0) known = 1;
    if (!known) { usage(argv[0]); return 2; }

    yyin = fopen(path, "r");
    if (!yyin) { perror(path); return 1; }

    if (strcmp(cmd, "trace") == 0)
        yydebug = 1;

    int result = yyparse();
    fclose(yyin);

    if (result != 0 || lex_errors || parse_errors) {
        fprintf(stderr, "%s: FAILED (%d lexical, %d syntax errors)\n",
                path, lex_errors, parse_errors);
        if (result == 0) ast_free(ast_root);
        return 1;
    }

    if (strcmp(cmd, "parse") == 0 || strcmp(cmd, "trace") == 0) {
        printf("%s: OK, syntax is correct\n", path);
        ast_free(ast_root);
        return 0;
    }

    int sem_errors = semantic_check(ast_root);
    if (sem_errors) {
        fprintf(stderr, "%s: FAILED (%d semantic errors)\n", path, sem_errors);
        ast_free(ast_root);
        return 1;
    }

    int rc = 0;
    if (strcmp(cmd, "ast") == 0) {
        ast_print(ast_root);
    } else if (strcmp(cmd, "check") == 0) {
        printf("%s: OK, syntax and semantics are correct\n", path);
    } else if (strcmp(cmd, "run") == 0) {
        rc = interpret(ast_root);
    } else if (strcmp(cmd, "py") == 0) {
        codegen_python(ast_root, path, stdout);
    } else if (strcmp(cmd, "json") == 0) {
        ast_export_json(ast_root, path, stdout);
    }

    ast_free(ast_root);
    return rc;
}
