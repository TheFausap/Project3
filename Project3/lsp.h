#pragma once

#ifndef _LSP_H
#define _LSP_H

//#define HT 
#define MAXENV 2000
#define MAXPACK 50

#include <stdio.h>
#include <math.h>
#include <string.h>
#ifndef _DEBUG
#include <mimalloc.h>
//#include <mimalloc-override.h>
#endif
#include "mpc.h"
#ifdef HT
#include "ht.h"
#endif

struct lval;
struct lenv;
struct pack;
typedef struct lval lval;
typedef struct lenv lenv;
typedef struct pack pack;

mpc_parser_t* Number;
mpc_parser_t* NumbI;
mpc_parser_t* NumbF;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;

#define LVER 2.0 /* in the form x.y */

/* lval types */
enum {
    LVAL_ERR = 0, LVAL_INUM, LVAL_DNUM, LVAL_SYM,
    LVAL_STR, LVAL_SEXPR, LVAL_FUN, LVAL_QEXPR
};

typedef lval* (*lbuiltin)(lenv*, lval*);

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_TYPE2(func, args, index, expect1, expect2) \
  LASSERT(args, (args->cell[index]->type == expect1) || (args->cell[index]->type == expect2), \
    "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s or %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect1), ltype_name(expect2))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);

typedef struct lval {
    int type;         /* 0 */
    intptr_t inum;        /* 1 */
    double dnum;      /* 2 */
    /* Error and Symbol types have some string data */
    char* err;
    char* sym;        /* 4 */
    char* str;        /* 5 */
    /* Functions */
    lbuiltin builtin; /* 6 (FFI?) */

    lenv* env;
    lval* formals;
    lval* body;
    /* Count and Pointer to a list of "lval*"; */
    int count;
    struct lval** cell;
} lval;

/* TODO: implement pacakges */
struct pack {
    lenv* env[2000];
    char* name;
    int pcount;
    int ecount;
    pack* plist[50];
};

struct lenv {
    lenv* par;
    int count;
#ifdef HT
    ht* h1;
#else
    char** syms;
    lval** vals;
#endif
};
#endif