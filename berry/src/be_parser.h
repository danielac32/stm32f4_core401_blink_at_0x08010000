#ifndef BE_PARSER_H
#define BE_PARSER_H

#include "be_object.h"
#include "be_string.h"

typedef enum {
    ETVOID,
    ETNIL,
    ETBOOL,
    ETREAL,
    ETINT,
    ETSTRING,
    ETPROTO,
    ETLOCAL,
    ETGLOBAL,
    ETUPVAL,
    ETMEMBER,
    ETINDEX,
    ETCONST,
    ETREG
} exptype_t;

typedef struct {
    union {
        struct { /* for suffix */
            unsigned int idx:9; /* suffix RK index */
            unsigned int obj:9; /* object RK index */
            unsigned int tt:5; /* object type */
        } ss;
        breal r;     /* for ETREAL */
        bint i;      /* for ETINT */
        bstring *s;  /* for ETSTRING */
        bproto *p;  /* for ETPROTO */
        int idx;     /* variable index */
    } v;
    void *extra;
    int t; /* patch list of 'exit when true' */
    int f; /* patch list of 'exit when false' */
    bbyte not; /* not mark */
    bbyte type;
} bexpdesc;

typedef struct bblockinfo {
    struct bblockinfo *prev;
    bbyte nactlocals; /* number of active local variables */
    bbyte isloop;     /* loop mark */
    bbyte hasupval;   /* has upvalue mark */
    int breaklist;    /* break list */
    int beginpc;      /* begin pc */
    int continuelist; /* continue list */
} bblockinfo;

typedef struct bfuncinfo {
    bproto *proto;
    struct bfuncinfo *prev;
    bblockinfo *binfo; /* block information */
    bvector code;
    blist *local; /* local variable */
    bmap *upval; /* upvalue variable */
    bvector *global; /* global variable */
    bvector kvec; /* constants table */
    bvector pvec; /* proto table */
    int pc; /* program count */
    int jpc;  /* list of pending jumps to 'pc' */
    bbyte nlocal; /* local variable count */
    bbyte nstack; /* stack usage */
    bbyte freereg; /* first free register */
    bbyte flag; /* anonymous function */
#if BE_RUNTIME_DEBUG_INFO /* debug information */
    struct blexer *lexer;
    bvector linevec;
#endif
} bfuncinfo;

bclosure* be_parser_source(bvm *vm,
    const char *fname, breader reader, void *data);

#endif
