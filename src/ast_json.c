#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast_json.h"
#include "codegen_py.h"
#include "interp.h"

static void json_string(FILE *out, const char *s)
{
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (*p < 0x20) fprintf(out, "\\u%04x", *p);
            else           fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void json_node(FILE *out, Node *n, int depth)
{
    fprintf(out, "%*s{\"kind\": ", depth * 2, "");
    json_string(out, kind_name(n->kind));
    fprintf(out, ", \"line\": %d", n->line);

    if (n->name && n->kind != N_STRING_LIT) {
        fputs(", \"name\": ", out);
        json_string(out, n->name);
    }
    if (n->uname && n->name && strcmp(n->uname, n->name) != 0) {
        fputs(", \"uname\": ", out);
        json_string(out, n->uname);
    }
    if (n->op) {
        fputs(", \"op\": ", out);
        json_string(out, n->op);
    }
    switch (n->kind) {
    case N_FUNC: case N_PARAM: case N_VAR_DECL:
        fputs(", \"decl_type\": ", out);
        json_string(out, type_name(n->type));
        break;
    case N_INCDEC:
        fprintf(out, ", \"prefix\": %s", n->prefix ? "true" : "false");
        break;
    case N_INT_LIT:
        fprintf(out, ", \"value\": %d", n->ival);
        break;
    case N_FLOAT_LIT:
        fprintf(out, ", \"value\": %.17g", n->dval);
        break;
    case N_STRING_LIT:
        fputs(", \"value\": ", out);
        json_string(out, n->name);
        break;
    default:
        break;
    }
    if (n->etype != T_UNKNOWN && n->kind != N_RETURN) {
        fputs(", \"type\": ", out);
        json_string(out, type_name(n->etype));
    }
    if (n->is_global)
        fputs(", \"global\": true", out);

    if (n->nkids > 0) {
        fputs(", \"children\": [\n", out);
        for (int i = 0; i < n->nkids; i++) {
            json_node(out, n->kids[i], depth + 1);
            fputs(i + 1 < n->nkids ? ",\n" : "\n", out);
        }
        fprintf(out, "%*s]", depth * 2, "");
    }
    fputc('}', out);
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    size_t got = fread(buf, 1, size, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

void ast_export_json(Node *program, const char *path, FILE *out)
{
    char  *py = NULL, *output = NULL;
    size_t py_len = 0, out_len = 0;

    FILE *ps = open_memstream(&py, &py_len);
    codegen_python(program, path, ps);
    fclose(ps);

    FILE *os = open_memstream(&output, &out_len);
    int exit_code = interpret_to(program, os);
    fclose(os);

    char *source = read_file(path);

    fputs("{\n\"file\": ", out);       json_string(out, path);
    fputs(",\n\"source\": ", out);     json_string(out, source);
    fputs(",\n\"python\": ", out);     json_string(out, py);
    fputs(",\n\"output\": ", out);     json_string(out, output);
    fprintf(out, ",\n\"exit_code\": %d", exit_code);
    fputs(",\n\"ast\":\n", out);
    json_node(out, program, 0);
    fputs("\n}\n", out);

    free(source);
    free(py);
    free(output);
}
