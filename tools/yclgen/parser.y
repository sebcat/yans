%pure-parser
%lex-param {void *scanner}
%parse-param {struct yclgen_ctx *ctx} {void *scanner}
%{

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "yclgen.h"
#include "parser.h"

#define YYERROR_VERBOSE


extern int yylex(YYSTYPE * lval, void *scanner);
extern int yylex_init(void *scanner);
extern int yylex_destroy(void *scanner);

static void push_msg(struct yclgen_ctx *ctx, const char *name);
static void push_field(struct yclgen_ctx *ctx, enum yclgen_field_type typ,
    const char *name);
static void free_msgs(struct yclgen_msg *msg);
static void yyerror(struct yclgen_ctx *ctx, void *scanner, const char *s);

%}

%union {
  char lit[32]; /* MAX_NAMELEN */
}

%token <lit> LIT
%token DATAARR
%token STRARR
%token LONGARR
%token DATA
%token STR
%token LONG

%%

defs:
  defs def
  | def
  ;

def:
  name '{' fields '}'
  | name '{' '}'
  ;

name:
  LIT  { push_msg(ctx, $1); }
  ;

fields:
  fields field
  | field
  ;

field:
  DATAARR LIT ';'   {push_field(ctx, FT_DATAARR, $2);}
  | STRARR LIT ';'  {push_field(ctx, FT_STRARR, $2);}
  | LONGARR LIT ';' {push_field(ctx, FT_LONGARR, $2);}
  | DATA LIT ';'    {push_field(ctx, FT_DATA, $2);}
  | STR LIT ';'     {push_field(ctx, FT_STR, $2);}
  | LONG LIT ';'    {push_field(ctx, FT_LONG, $2);}
  ;

%%

int yclgen_parse(struct yclgen_ctx *ctx) {
  int ret;
  void *scanner;

  memset(ctx, 0, sizeof(*ctx));
  yylex_init(&scanner);
  ret = yyparse(ctx, scanner);
  yylex_destroy(scanner);
  return ret;
}

void yclgen_cleanup(struct yclgen_ctx *ctx) {
  if (ctx && ctx->latest_msg) {
    free_msgs(ctx->latest_msg);
  }
}

void push_msg(struct yclgen_ctx *ctx, const char *name) {
  struct yclgen_msg *m;

  m = malloc(sizeof(struct yclgen_msg));
  if (!m) {
    fprintf(stderr, "out of memory while allocating msg struct\n"),
    exit(1);
  }

  snprintf(m->name, sizeof(m->name), "%s", name);
  m->nfields = 0;
  m->next = ctx->latest_msg;
  ctx->latest_msg = m;
}

void push_field(struct yclgen_ctx *ctx, enum yclgen_field_type typ,
    const char *name) {
  struct yclgen_msg *m;
  struct yclgen_field *f;

  m = ctx->latest_msg;
  if (!m) {
    fprintf(stderr, "attempt to push field on empty message\n");
    exit(1);
  }

  if (m->nfields >= MAX_FIELDS) {
    fprintf(stderr, "too many fields in message \"%s\"\n", m->name);
    exit(1);
  }

  f = &m->fields[m->nfields];
  f->typ = typ;
  snprintf(f->name, sizeof(f->name), "%s", name);
  m->nfields++;
  switch (typ) {
  case FT_DATAARR:
    m->flags |= MSGF_HASDARR;
    ctx->flags |= MSGF_HASDARR;
    break;
  case FT_STRARR:
    m->flags |= MSGF_HASSARR;
    ctx->flags |= MSGF_HASSARR;
    break;
  case FT_LONGARR:
    m->flags |= MSGF_HASLARR;
    ctx->flags |= MSGF_HASLARR;
    break;
  case FT_LONG:
    m->flags |= MSGF_HASLONG;
    ctx->flags |= MSGF_HASLONG;
    break;
  case FT_STR:
    m->flags |= MSGF_HASSTR;
    ctx->flags |= MSGF_HASSTR;
    break;
  case FT_DATA:
    m->flags |= MSGF_HASDATA;
    ctx->flags |= MSGF_HASDATA;
    break;
  }
}

void free_msgs(struct yclgen_msg *msg) {
  struct yclgen_msg *curr;
  struct yclgen_msg *next;

  for (curr = msg; curr != NULL; curr = next) {
    next = curr->next;
    free(curr);
  }
}

void yyerror(struct yclgen_ctx *ctx, void *scanner, const char *s) {
  (void)ctx;
  fprintf(stderr, "parse error: %s\n", s);
  exit(1);
}

