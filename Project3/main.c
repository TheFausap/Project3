#include "lsp.h"
#include <varargs.h>
#include <time.h>

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

int gensym = 0;

bignum BZERO;

/* Simple package implementation */
/* Global vars to simulate the IN-PACKAGE */
/* and the USE-PACKAGE commands */
pack* rootpack = NULL;
pack* currpack = NULL;
pack** packlist = NULL;

/* Construct a pointer to a new Number lval */
lval* lval_inum(intptr_t x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_INUM;
    v->inum = x;
    return v;
}

/* Construct a pointer to a new Number lval */
lval* lval_dnum(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_DNUM;
    v->dnum = x;
    return v;
}

lval* lval_bnum(bignum b) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_BNUM;
    initialize_bignum(&v->bnum); /* init to 0 */
    add_bignum(&b, &BZERO, &v->bnum);
    return v;
}

lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, fmt, va);

    /* Reallocate to number of bytes actually used */
    v->err = realloc(v->err, strlen(v->err) + 1);

    /* Cleanup our va list */
    va_end(va);

    return v;
}

/* Construct a pointer to a new Symbol lval */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* A pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_builtin(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
}

lval* lval_str(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_STR;
    v->str = malloc(strlen(s) + 1);
    strcpy(v->str, s);
    return v;
}

void pack_envadd(pack* p, lenv* e);

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
#ifdef HT
    e->h1 = ht_create();
#else
    e->syms = NULL;
    e->vals = NULL;
#endif
    pack_envadd(currpack, e);
    return e;
}

/* create the root package */
pack* pack_init(void) {
    pack* p = malloc(sizeof(pack));
    p->pcount = 0; /* this is the number of packages in the root */
    p->ecount = 0;
    p->name = ":LSPY";
    p->env[0] = NULL;
    p->plist[0] = NULL;
    rootpack = p;
    currpack = p;
    return p;
}

/* add a new package to the root */
pack* pack_new(char *n) {
    if (rootpack->pcount > MAXPACK) {
        fprintf(stderr, "E100");
        exit(-100);
    }
    pack* pn = malloc(sizeof(pack));
    pn->ecount = 0; /* this is the number of envs in the package */
    pn->pcount = 0;
    pn->name = n;
    pn->env[0] = NULL;
    pn->plist[pn->pcount] = NULL; /* no sub-packages */
    return pn;
}

pack* pack_copy(pack* e) {
    pack* n = malloc(sizeof(pack));
    n->ecount = e->ecount; /* this is the number of envs in the package */
    n->pcount = e->pcount;
    n->name = e->name;
    for (int i = 0; i < n->ecount; i++)
        n->env[i] = e->env[i];
    n->plist[n->pcount] = NULL; /* no sub-packages */
    return n;
}

/* add a reference to an environment to a specific package */
void pack_envadd(pack* p, lenv* e) {
    
    if (p->ecount > MAXENV) {
        fprintf(stderr, "E200");
        exit(-200);
    }
    p->env[p->ecount] = e; /* reference directly the memory address */
                           /* do not want a copy */
    p->ecount++;
}

/* add a reference to an environment to a specific package */
int pack_envdel(pack* p, lenv* e) {
    int i = 0;
    while (i <= p->ecount) {
        if (p->env[i] == e) break;
        i++;
    }
    if (i > p->ecount) { return -1; }; /* temp env not added to package */

    p->env[i] = NULL;
    p->ecount--;
    if (p->ecount < 0) {
        fprintf(stderr, "E210");
        exit(-210);
    }
    return i;
}
 
void lval_del(lval* v);

void lenv_del(lenv* e) {
#ifdef HT
    ht_destroy(e->h1);
#else
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    pack_envdel(currpack, e);
    free(e->syms);
    free(e->vals);
#endif
    free(e);
}

lval* lval_copy(lval* v);

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
#ifdef HT
    n->h1 = ht_create();
#else
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
#endif

#ifdef HT
    hti ite = ht_iterator(e->h1);
    while (ht_next(&ite)) {
        ht_set(n->h1, ite.key, lval_copy(ite.value));
    }
#else
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
#endif
    return n;
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    /* Set Builtin to Null */
    v->builtin = NULL;

    /* Build new environment */
    v->env = lenv_new();

    /* Set Formals and Body */
    v->formals = formals;
    v->body = body;
    return v;
}

void lval_del(lval* v) {

    switch (v->type) {
        /* Do nothing special for number type */
    case LVAL_INUM: break;
    case LVAL_DNUM: break;
    case LVAL_FUN:
        if (!v->builtin) {
            lenv_del(v->env);
            lval_del(v->formals);
            lval_del(v->body);
        }
        break;

        /* For Err or Sym free the string data */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    case LVAL_STR: free(v->str); break;

        /* If Sexpr then delete all elements inside */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
            lval_del(v->cell[i]);
        }
        /* Also free the memory allocated to contain the pointers */
        free(v->cell);
        break;
    }

    /* Free the memory allocated for the "lval" struct itself */
    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_pop(lval* v, int i) {
    /* Find the item at "i" */
    lval* x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i + 1],
        sizeof(lval*) * (v->count - i - 1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_copy(lval* v) {

    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {

        /* Copy Functions and Numbers Directly */
    case LVAL_FUN:
        if (v->builtin) {
            x->builtin = v->builtin;
        }
        else {
            x->builtin = NULL;
            x->env = lenv_copy(v->env);
            x->formals = lval_copy(v->formals);
            x->body = lval_copy(v->body);
        }
        break;
    case LVAL_INUM: x->inum = v->inum; break;
    case LVAL_DNUM: x->dnum = v->dnum; break;
    case LVAL_BNUM: add_bignum(&v->bnum, &BZERO, &x->bnum); break;

        /* Copy Strings using malloc and strcpy */
    case LVAL_ERR:
        x->err = malloc(strlen(v->err) + 1);
        strcpy(x->err, v->err); break;

    case LVAL_SYM:
        x->sym = malloc(strlen(v->sym) + 1);
        strcpy(x->sym, v->sym); break;

    case LVAL_STR: 
        x->str = malloc(strlen(v->str) + 1);
        strcpy(x->str, v->str);
        break;
        /* Copy Lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
        x->count = v->count;
        x->cell = malloc(sizeof(lval*) * x->count);
        for (int i = 0; i < x->count; i++) {
            x->cell[i] = lval_copy(v->cell[i]);
        }
        break;
    }

    return x;
}

lval* lenv_get(lenv* e, lval* k) {
#ifdef HT
    lval* x;

    if ((x = ht_get(e->h1, k->sym)) != NULL) {
        return lval_copy(x);
    }
#else
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
#endif

    /* If no symbol check in parent otherwise error */
    if (e->par) {
        return lenv_get(e->par, k);
    }
    else {
        return lval_err("Unbound Symbol '%s'", k->sym);
    }
}

void lenv_put(lenv* e, lval* k, lval* v) {
#ifdef HT
        ht_set(e->h1, k->sym, lval_copy(v));
#else
    /* Iterate over all items in environment */
    /* This is to see if variable already exists */
    for (int i = 0; i < e->count; i++) {

        /* If variable is found delete item at that position */
        /* And replace with variable supplied by user */
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry found allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
#endif
}

void lenv_def(lenv* e, lval* k, lval* v) {
    /* Iterate till e has no parent */
    while (e->par) { e = e->par; }
    /* Put value in e */
    lenv_put(e, k, v);
}

lval* builtin_eval(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);

lval* lval_call(lenv* e, lval* f, lval* a) {

    /* If Builtin then simply apply that */
    if (f->builtin) { return f->builtin(e, a); }

    /* Record Argument Counts */
    int given = a->count;
    int total = f->formals->count;

    /* While arguments still remain to be processed */
    while (a->count) {

        /* If we've ran out of formal arguments to bind */
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function passed too many arguments. "
                "Got %i, Expected %i.", given, total);
        }

        /* Pop the first symbol from the formals */
        lval* sym = lval_pop(f->formals, 0);

        /* Special Case to deal with '&' */
        if (strcmp(sym->sym, "&") == 0) {

            /* Ensure '&' is followed by another symbol */
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. "
                    "Symbol '&' not followed by single symbol.");
            }

            /* Next formal should be bound to remaining arguments */
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym); lval_del(nsym);
            break;
        }

        /* Pop the next argument from the list */
        lval* val = lval_pop(a, 0);

        /* Bind a copy into the function's environment */
        lenv_put(f->env, sym, val);

        /* Delete symbol and value */
        lval_del(sym); lval_del(val);
    }

    /* Argument list is now bound so can be cleaned up */
    lval_del(a);

    /* If '&' remains in formal list bind to empty list */
    if (f->formals->count > 0 &&
        strcmp(f->formals->cell[0]->sym, "&") == 0) {

        /* Check to ensure that & is not passed invalidly. */
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                "Symbol '&' not followed by single symbol.");
        }

        /* Pop and delete '&' symbol */
        lval_del(lval_pop(f->formals, 0));

        /* Pop next symbol and create empty list */
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        /* Bind to environment and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    /* If all formals have been bound evaluate */
    if (f->formals->count == 0) {

        /* Set environment parent to evaluation environment */
        f->env->par = e;

        /* Evaluate and return */
        return builtin_eval(f->env,
            lval_add(lval_sexpr(), lval_copy(f->body)));
    }
    else {
        /* Otherwise return partially evaluated function */
        return lval_copy(f);
    }

}

void lval_print(lval* v);
lval* lval_eval(lenv*e, lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        /* Print Value contained within */
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print_str(lval* v) {
    /* Make a Copy of the string */
    char* escaped = malloc(strlen(v->str) + 1);
    strcpy(escaped, v->str);
    /* Pass it through the escape function */
    escaped = mpcf_escape(escaped);
    /* Print it between " characters */
    printf("\"%s\"", escaped);
    /* free the copied string */
    free(escaped);
}

void lval_print(lval* v) {
    switch (v->type) {
    case LVAL_INUM:  printf("%lli", v->inum); break;
    case LVAL_DNUM:  printf("%lf", v->dnum); break;
    case LVAL_BNUM:
        print_bignum(&v->bnum);
        break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_STR:   lval_print_str(v); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    case LVAL_FUN:
        if (v->builtin) {
            printf("<builtin@%p>",v->builtin);
        }
        else {
            printf("(\\ "); lval_print(v->formals);
            putchar(' '); lval_print(v->body); putchar(')');
        }
        break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

char* ltype_name(int t) {
    switch (t) {
    case LVAL_FUN: return "Function";
    case LVAL_INUM: return "Integer Number";
    case LVAL_DNUM: return "Floating-Point Number";
    case LVAL_BNUM: return "BIGNUM";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_STR: return "String";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
    }
}


lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);

    lval* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

/* This is like the CL QUOTE */
lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {

    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

int lval_eq(lval* x, lval* y) {
    double d1, d2;

    /* Different Types are always unequal */
    if (x->type != y->type) { return 0; }

    /* Compare Based upon type */
    switch (x->type) {
        /* Compare Number Value */
    case LVAL_DNUM:
    case LVAL_INUM:
        d1 = (x->type == LVAL_INUM) ? (double)x->inum : x->dnum;
        d2 = (y->type == LVAL_INUM) ? (double)y->inum : y->dnum;
        return (d1 == d2);

        /* Compare String Values */
    case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
    case LVAL_STR: return (strcmp(x->str, y->str) == 0);

        /* If builtin compare, otherwise compare formals and body */
    case LVAL_FUN:
        if (x->builtin || y->builtin) {
            return x->builtin == y->builtin;
        }
        else {
            return lval_eq(x->formals, y->formals)
                && lval_eq(x->body, y->body);
        }

        /* If list compare every individual element */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
        if (x->count != y->count) { return 0; }
        for (int i = 0; i < x->count; i++) {
            /* If any element not equal then whole list not equal */
            if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
        }
        /* Otherwise lists must be equal */
        return 1;
        break;
    }
    return 0;
}

lval* builtin_join(lenv* e, lval* a) {

    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_cons(lenv*e, lval* a) {
    LASSERT_NUM("cons", a, 2);
    LASSERT_TYPE("cons", a, 0, LVAL_QEXPR);

    lval* x = lval_pop(a, 0);
    x = lval_add(x, lval_pop(a, 0));
    lval_del(a);
    return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    double d1;
    short is_dop = 0;
    lval* r;

    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if ((a->cell[i]->type != LVAL_INUM) && (a->cell[i]->type != LVAL_DNUM)) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        if (x->type == LVAL_INUM) {
            x->inum = -x->inum;
        }
        else {
            x->dnum = -x->dnum;
        }
    }

    if ((strcmp(op, "/") == 0) && a->count == 0) {
        if (x->type == LVAL_INUM) {
            x->inum = 1 / x->inum;
        }
        else {
            x->dnum = 1.0 / x->dnum;
        }
    }

    if ((strcmp(op, "^") == 0) && a->count == 0) {
        if (x->type == LVAL_INUM) {
            x->inum = (int)pow(2.0,(double)x->inum);
        }
        else {
            x->dnum = pow(2.0,x->dnum);
        }
    }

    if (x->type == LVAL_DNUM) {
        is_dop = 1;
        d1 = x->dnum;
    }
    else {
        d1 = (double)x->inum;
    }

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        /* Perform operation */
        if (strcmp(op, "+") == 0) {
            if (y->type == LVAL_DNUM) {
                d1 += y->dnum;
                is_dop = 1;
            }
            else {
                d1 += (double)y->inum;
            }
        }
        if (strcmp(op, "-") == 0) {
            if (y->type == LVAL_DNUM) {
                d1 -= y->dnum;
                is_dop = 1;
            }
            else {
                d1 -= (double)y->inum;
            }
        }
        if (strcmp(op, "*") == 0) {
            if (y->type == LVAL_DNUM) {
                d1 *= y->dnum;
                is_dop = 1;
            }
            else {
                d1 *= (double)y->inum;
            }
        }
        if (strcmp(op, "/") == 0) {
            if (y->type == LVAL_INUM) {
                if (y->inum == 0) {
                    lval_del(x); lval_del(y);
                    r = lval_err("Division By Zero.");
                    return r;
                }
            }
            else {
                if (y->dnum == 0.0) {
                    lval_del(x); lval_del(y);
                    r = lval_err("Division By Zero.");
                    return r;
                }
            }
            
            if (y->type == LVAL_DNUM) {
                d1 /= y->dnum;
                is_dop = 1;
            }
            else {
                d1 /= (double)y->inum;
            }
        }

        if (strcmp(op, "^") == 0) {
            if (y->type == LVAL_INUM) {
                d1 = pow(d1, (double)y->inum);
            }
            else {
                d1 = pow(d1, y->dnum);
            }
        }

        if (strcmp(op, "%") == 0) {
            if (y->type == LVAL_INUM) {
                d1 = fmod(d1,(double)y->inum);
            }
            else {
                d1 = fmod(d1,y->dnum);
            }
        }

        /* Delete element now finished with */
        lval_del(y);
    }

    /* Delete input expression and return result */
    lval_del(a);
    if (is_dop) {
        r = lval_dnum(d1);
    }
    else {
        r = lval_inum((int)d1);
    }
    return r;
}

lval* builtin_lambda(lenv* e, lval* a) {
    /* Check Two arguments, each of which are Q-Expressions */
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    /* Check first Q-Expression contains only Symbols */
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    /* Pop first two arguments and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_pow(lenv* e, lval* a) {
    return builtin_op(e, a, "^");
}

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "%");
}

lval* builtin_addb(lenv* e, lval* a) {
    lval* r;
    bignum b, c, tmp;

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    if (x->type == LVAL_INUM)
        int_to_bignum(x->inum, &b);
    else
        add_bignum(&x->bnum,&BZERO,&b);

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if (y->type == LVAL_INUM)
            int_to_bignum(y->inum, &c);
        else
            add_bignum(&y->bnum,&BZERO,&c);

        add_bignum(&b, &c, &tmp);
        add_bignum(&tmp, &BZERO, &b);

        lval_del(y);
    }

    lval_del(a); 
    r = lval_bnum(b);
    return r;
}

lval* builtin_subb(lenv* e, lval* a) {
    lval* r;
    bignum b, c, tmp;

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    if (x->type == LVAL_INUM)
        int_to_bignum(x->inum, &b);
    else
        add_bignum(&x->bnum, &BZERO, &b);

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if (y->type == LVAL_INUM)
            int_to_bignum(y->inum, &c);
        else
            add_bignum(&y->bnum, &BZERO, &c);

        subtract_bignum(&b, &c, &tmp);
        add_bignum(&tmp, &BZERO, &b);

        lval_del(y);
    }

    lval_del(a); 
    r = lval_bnum(b);
    return r;
}

lval* builtin_mulb(lenv* e, lval* a) {
    lval* r;
    bignum b, c, tmp;

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    if (x->type == LVAL_INUM)
        int_to_bignum(x->inum, &b);
    else
        add_bignum(&x->bnum, &BZERO, &b);

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if (y->type == LVAL_INUM)
            int_to_bignum(y->inum, &c);
        else
            add_bignum(&y->bnum, &BZERO, &c);

        multiply_bignum(&b, &c, &tmp);
        add_bignum(&tmp, &BZERO, &b);

        lval_del(y);
    }

    lval_del(a); 
    r = lval_bnum(b);
    return r;
}

lval* builtin_divb(lenv* e, lval* a) {
    lval* r;
    bignum b, c, tmp;

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    if (x->type == LVAL_INUM)
        int_to_bignum(x->inum, &b);
    else
        add_bignum(&x->bnum, &BZERO, &b);

    /* While there are still elements remaining */
    while (a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if (y->type == LVAL_INUM)
            int_to_bignum(y->inum, &c);
        else
            add_bignum(&y->bnum, &BZERO, &c);

        divide_bignum(&b, &c, &tmp);
        add_bignum(&tmp, &BZERO, &b);

        lval_del(y);
    }

    lval_del(a); 
    r = lval_bnum(b);
    return r;
}

lval* builtin_i_to_bnum(lenv* e, lval* a) {
    LASSERT_NUM("to-bnum", a, 1);
    LASSERT_TYPE("to-bnum", a, 0, LVAL_INUM);
    lval* x = lval_bnum(BZERO);

    int_to_bignum(a->cell[0]->inum, &x->bnum);
    return x;
}

/* 0 == eq, 1 == a < b, -1 == a > b */
lval* builtin_cmp_bnum(lenv* e, lval* a) {
    LASSERT_NUM("cmp-bnum", a, 2);
    LASSERT_TYPE("cmp-bnum", a, 0, LVAL_BNUM);
    LASSERT_TYPE("cmp-bnum", a, 1, LVAL_BNUM);
    int v;

    v = compare_bignum(&a->cell[0]->bnum, &a->cell[1]->bnum);
    return lval_inum(v);
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
    double d1, d2;

    LASSERT_NUM(op, a, 2);
    LASSERT_TYPE2(op, a, 0, LVAL_INUM, LVAL_DNUM);
    LASSERT_TYPE2(op, a, 1, LVAL_INUM, LVAL_DNUM);

    int r;
    d1 = (a->cell[0]->type == LVAL_INUM) ? (double)a->cell[0]->inum : a->cell[0]->dnum;
    d2 = (a->cell[1]->type == LVAL_INUM) ? (double)a->cell[1]->inum : a->cell[1]->dnum;

    if (strcmp(op, ">") == 0) {
        r = (d1 > d2);
    }
    if (strcmp(op, "<") == 0) {
        r = (d1 < d2);
    }
    if (strcmp(op, ">=") == 0) {
        r = (d1 >= d2);
    }
    if (strcmp(op, "<=") == 0) {
        r = (d1 <= d2);
    }
    lval_del(a);
    return lval_inum(r);
}

lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    int r;
    if (strcmp(op, "==") == 0) {
        r = lval_eq(a->cell[0], a->cell[1]);
    }
    if (strcmp(op, "!=") == 0) {
        r = !lval_eq(a->cell[0], a->cell[1]);
    }
    lval_del(a);
    return lval_inum(r);
}

lval* builtin_eq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
    return builtin_cmp(e, a, "!=");
}

lval* builtin_if(lenv* e, lval* a) {
    LASSERT_NUM("if", a, 3);
    LASSERT_TYPE2("if", a, 0, LVAL_INUM, LVAL_DNUM);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

    /* Mark Both Expressions as evaluable */
    lval* x;
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    if (a->cell[0]->inum) {
        /* If condition is true evaluate first expression */
        x = lval_eval(e, lval_pop(a, 1));
    }
    else {
        /* Otherwise evaluate second expression */
        x = lval_eval(e, lval_pop(a, 2));
    }

    /* Delete argument list and return */
    lval_del(a);
    return x;
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.", func,
            ltype_name(syms->cell[i]->type),
            ltype_name(LVAL_SYM));
    }

    LASSERT(a, (syms->count == a->count - 1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count - 1);

    for (int i = 0; i < syms->count; i++) {
        /* If 'def' define in globally. If 'put' define in locally */
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }

        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
    //return syms->cell[0];
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* builtin_penv(lenv* e, lval* a) {
#ifdef HT
    hti it = ht_iterator(e->h1);
    while (ht_next(&it)) {
        switch (((lval*)it.value)->type) {
        case LVAL_INUM:
            printf("(%s %lli)\n", it.key, ((lval*)it.value)->inum);
            break;
        case LVAL_DNUM:
            printf("(%s %lf)\n", it.key, ((lval*)it.value)->dnum);
            break;
        case LVAL_SYM:
            printf("(%s %s)\n", it.key, ((lval*)it.value)->sym);
            break;
        case LVAL_FUN:
            printf("(%s {func})\n", it.key);
            break;
        }
    }
#else
    LASSERT_NUM("printenv", a, 1);
    for (int i = 0; i < e->count; i++) {
        switch (e->vals[i]->type) {
        case LVAL_INUM:
            printf("(%s %lli)\n", e->syms[i], e->vals[i]->inum);
            break;
        case LVAL_DNUM:
            printf("(%s %lf)\n", e->syms[i], e->vals[i]->dnum);
            break;
        case LVAL_SYM:
            printf("(%s %s)\n", e->syms[i], e->vals[i]->sym);
            break;
        case LVAL_FUN:
            printf("(%s {func})\n", e->syms[i]);
            break;
        } 
    }
#endif
    lval_del(a);
    return lval_sexpr();
}

lval* builtin_dpb(lenv* e, lval* a) {
    LASSERT_NUM("dpb", a, 3);
    LASSERT_TYPE("dpb", a, 1, LVAL_INUM); /* LVAL elem */
    switch (a->cell[1]->inum) {
    case 0:
        LASSERT_TYPE("dpb", a, 2, LVAL_INUM);
        a->cell[0]->type = (int) a->cell[2]->inum;
        break;
    case 1:
        LASSERT_TYPE("dpb", a, 2, LVAL_INUM);
        a->cell[0]->inum = a->cell[2]->inum;
        break;
    case 2:
        LASSERT_TYPE("dpb", a, 2, LVAL_DNUM);
        a->cell[0]->dnum = a->cell[2]->dnum;
        break;
    case 4:
        LASSERT_TYPE("dpb", a, 2, LVAL_SYM);
        a->cell[0]->sym = a->cell[2]->sym;
        break;
    case 5:
        LASSERT_TYPE("dpb", a, 2, LVAL_STR);
        a->cell[0]->str = a->cell[2]->str;
        break;
    case 6: /* useful for a sort of FFI ? */
        LASSERT_TYPE("dpb", a, 2, LVAL_FUN);
        /* Is a good idea change a builtin? It could be fun */
        if (a->cell[2]->builtin) {
            a->cell[0]->builtin = a->cell[2]->builtin;
        }
        else {
            a->cell[0]->formals = a->cell[2]->formals;
            a->cell[0]->body = a->cell[2]->body;
        }
        break;
    default:
        return lval_err("Unhandled lval type");
    }
    lval_del(a);
    return lval_sexpr();
}

/* (ldb x 0) - returns the type value (byte 0) of variable x */
lval* builtin_ldb(lenv* e, lval* a) {
    LASSERT_NUM("ldb", a, 2);
    LASSERT_TYPE("ldb", a, 1, LVAL_INUM);
    switch (a->cell[1]->inum) {
    case 0:
        return lval_inum(a->cell[0]->type);
    case 1:
        return lval_inum(a->cell[0]->inum);
    case 2:
        return lval_dnum(a->cell[0]->dnum);
    case 4:
        lval_print(a->cell[0]);
        break;
    case 5:
        lval_print(a->cell[0]);
        break;
    case 6:
        if (a->cell[0]->builtin) {
            return lval_inum((intptr_t)a->cell[0]->builtin);
        }
        else {
            lval* r = lval_qexpr();
            r->cell[0] = lval_inum((intptr_t)a->cell[0]->formals);
            r->cell[1] = lval_inum((intptr_t)a->cell[0]->body);
            return r;
        }
    default:
        return lval_err("Unhandled lval type");
    }
    lval_del(a);
    return lval_sexpr();
}

/* generate q-expr (as symbols used in a def or =) */
lval* builtin_gsym(lenv* e, lval* a) {
    char* s;
    char* g;
    int l = 0;

    s = malloc(20*sizeof(char));
    if (a->cell[0]->type == LVAL_STR) {
        s[0] = a->cell[0]->str[0];
    }
    else {
        s[0] = 'g';
    }
    g = malloc(4 * sizeof(char));
    l = sprintf(g, "%d", gensym);
    for (int i=0;i<l;i++) s[i+1] = g[i];
    s[l+1] = '\0';
    lval* x = lval_qexpr();
    x->count = 1;
    x->cell = malloc(sizeof(lval));
    x->cell[0]=lval_sym(s);
    gensym++;

    lval_del(a);
    return x;
}

lval* builtin_range(lenv* e, lval* a) {
    LASSERT_TYPE("range", a, 0, LVAL_INUM);
    LASSERT_TYPE("range", a, 1, LVAL_INUM);
    LASSERT_NUM("range", a, 2);


    intptr_t rmin = a->cell[0]->inum;
    intptr_t rmax = a->cell[1]->inum;
    lval* x = lval_qexpr();
    x->count = (int) (rmax - rmin);
    x->cell = malloc((x->count) * sizeof(lval));
    for (intptr_t i = rmin; i < rmax; i++) {
        x->cell[i-rmin] = lval_inum(i);
    }

    lval_del(a);
    return x;
}

lval* builtin_random(lenv* e, lval* a) {
    LASSERT_TYPE("random", a, 0, LVAL_INUM);
    LASSERT_NUM("random", a, 1);
    srand((unsigned)time(NULL));
    lval* x = lval_inum((intptr_t)rand() % a->cell[0]->inum);
    lval_del(a);
    return x;
}

lval* builtin_exit(lenv* e, lval* a) {
    /* More than one number as argument is possible, 
       but it will be ignored */
    LASSERT_TYPE("exit", a, 0, LVAL_INUM);

    if (a->count == 0) {
#ifndef _DEBUG
        mi_stats_print(NULL);
#endif
        exit(0);
    }
    else {
        lval* x = lval_pop(a, 0);
#ifndef _DEBUG
        mi_stats_print(NULL);
#endif
        exit((int) x->inum);
    }
}

lval* lval_read(mpc_ast_t* t);

lval* builtin_load(lenv* e, lval* a) {
    LASSERT_NUM("load", a, 1);
    LASSERT_TYPE("load", a, 0, LVAL_STR);

    /* Parse File given by string name */
    mpc_result_t r;
    if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {

        /* Read contents */
        lval* expr = lval_read(r.output);
        mpc_ast_delete(r.output);

        /* Evaluate each Expression */
        while (expr->count) {
            lval* x = lval_eval(e, lval_pop(expr, 0));
            /* If Evaluation leads to error print it */
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }

        /* Delete expressions and arguments */
        lval_del(expr);
        lval_del(a);

        /* Return empty list */
        return lval_sexpr();

    }
    else {
        /* Get Parse Error as String */
        char* err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);

        /* Create new error message using it */
        lval* err = lval_err("Could not load Library %s", err_msg);
        free(err_msg);
        lval_del(a);

        /* Cleanup and return error */
        return err;
    }
}

lval* builtin_print(lenv* e, lval* a) {

    /* Print each argument followed by a space */
    for (int i = 0; i < a->count; i++) {
        lval_print(a->cell[i]); putchar(' ');
    }

    /* Print a newline and delete arguments */
    putchar('\n');
    lval_del(a);

    return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
    LASSERT_NUM("error", a, 1);
    LASSERT_TYPE("error", a, 0, LVAL_STR);

    /* Construct Error from first argument */
    lval* err = lval_err(a->cell[0]->str);

    /* Delete arguments and return */
    lval_del(a);
    return err;
}

lval* builtin_readline(lenv* e, lval* a) {
    char* prompt;
    char uinput[50];

    if (a->cell[0]->type == LVAL_STR) {
        prompt = a->cell[0]->str;
    }
    else {
        prompt = malloc(2*sizeof(char));
        prompt[0] = '?';
        prompt[1] = '\0';
    }

    fprintf(stdout, "%s ", prompt);
    fgets(uinput,50,stdin);
    uinput[strlen(uinput) - 1] = '\0';
    
    lval_del(a);
    return lval_str(uinput);
}

/* create a new package and put it in the root plist */
/* DO NOT set the currpack */
lval* builtin_makepack(lenv* e, lval* a) {
    LASSERT_NUM("make-package", a, 1);
    LASSERT_TYPE("make-package", a, 0, LVAL_STR);
    pack* x = pack_new(a->cell[0]->str);

    rootpack->plist[rootpack->pcount] = pack_copy(x);
    rootpack->pcount++;
    //lval_del(a);
    return lval_sexpr();
}

lval* builtin_usepack(lenv* e, lval* a) {
    int i = 0;
    LASSERT_NUM("use-package", a, 1);
    LASSERT_TYPE("use-package", a, 0, LVAL_STR);

    while  (i < rootpack->pcount) {
        if (rootpack->plist[i]->name == a->cell[0]->str) break;
        i++;
    }

    if ((i == rootpack->pcount) || (rootpack->pcount == -1)) 
        return lval_err("Package %s not found.", a->cell[0]->str);

    currpack = rootpack->plist[i];

    lval_del(a);
    return lval_sexpr();
}

lval* builtin_listpack(lenv* e, lval* a) {
    lval* x = lval_qexpr();
    lval* t = lval_str(":LSPY");
    lval_add(x, lval_copy(t));

    if (rootpack->pcount > -1) {
        for (int i = 0; i < rootpack->pcount; i++) {
            t = lval_str(rootpack->plist[i]->name);
            lval_add(x, t);
        }
    }

    lval_del(a); 
    //lval_del(t);
    return x;
}

/* End of BUILTINS */

lval* lval_eval_sexpr(lenv* e, lval* v) {

    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    if (v->count == 0) { return v; } /* No func with 0 args allowed */
    if (v->count == 1) { return lval_take(v, 0); }

    /* Ensure first element is a function after evaluation */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f); lval_del(v);
        return err;
    }

    lval* result = lval_call(e, f, v);

    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}

lval* lval_read_inum(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_inum(x) : lval_err("invalid number");
}

lval* lval_read_dnum(mpc_ast_t* t) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ?
        lval_dnum(x) : lval_err("invalid number");
}

lval* lval_read_str(mpc_ast_t* t) {
    /* Cut off the final quote character */
    t->contents[strlen(t->contents) - 1] = '\0';
    /* Copy the string missing out the first quote character */
    char* unescaped = malloc(strlen(t->contents + 1) + 1);
    strcpy(unescaped, t->contents + 1);
    /* Pass through the unescape function */
    unescaped = mpcf_unescape(unescaped);
    /* Construct a new lval using the string */
    lval* str = lval_str(unescaped);
    /* Free the string and return */
    free(unescaped);
    return str;
}

lval* lval_read(mpc_ast_t* t) {

    /* If Symbol or Number return conversion to that type */
    if (strstr(t->tag, "numbI")) { return lval_read_inum(t); }
    if (strstr(t->tag, "numbF")) { return lval_read_dnum(t); }
    if (strstr(t->tag, "string")) { return lval_read_str(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
    

    /* If root (>) or sexpr then create empty list */
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        if (strstr(t->children[i]->tag, "comment")) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_builtin(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);

    /* Debug / Internal Functions */
    lenv_add_builtin(e, "printenv", builtin_penv);
    lenv_add_builtin(e, "error", builtin_error);
    lenv_add_builtin(e, "print", builtin_print);
    lenv_add_builtin(e, "dpb", builtin_dpb);
    lenv_add_builtin(e, "ldb", builtin_ldb);
    lenv_add_builtin(e, "make-package", builtin_makepack);
    lenv_add_builtin(e, "use-package", builtin_usepack);
    lenv_add_builtin(e, "list-package", builtin_listpack);

    /* Variable Functions */
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "gensym", builtin_gsym);
    lenv_add_builtin(e, "range", builtin_range);


    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "^", builtin_pow);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "random", builtin_random);
    /* integer bignum */
    lenv_add_builtin(e, "addb", builtin_addb);
    lenv_add_builtin(e, "subb", builtin_subb);
    lenv_add_builtin(e, "mulb", builtin_mulb);
    lenv_add_builtin(e, "divb", builtin_divb);
    /* conversion */
    lenv_add_builtin(e, "to-bnum", builtin_i_to_bnum);

    /* Comparison Functions */
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, "<", builtin_lt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<=", builtin_le);

    lenv_add_builtin(e, "cmp-bnum", builtin_cmp_bnum);

    /* OS level Functions */
    lenv_add_builtin(e, "exit", builtin_exit);
    lenv_add_builtin(e, "load", builtin_load);
    lenv_add_builtin(e, "read-line", builtin_readline);
}

int main(int argc, char** argv) {
    int lisp_version = 0;
    int lisp_build = 0;
    int mv = 0;
    initialize_bignum(&BZERO);

    lisp_version = (int)hypot(LVER * 42.0, 42.0);
    lisp_build = (int)(100000*(hypot(LVER + 42.0, 42.0) - (int)hypot(LVER + 42.0, 42.0)));
#ifndef _DEBUG
    mv = mi_version();
#endif

    Number = mpc_new("number");
    NumbI = mpc_new("numbI");
    NumbF = mpc_new("numbF");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    Symbol = mpc_new("symbol");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Expr = mpc_new("expr");
    Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                           \
      numbF  : /-?[0-9]+\\.[0-9]+/ ;                \
      numbI  : /-?[0-9]+/ ;                         \
      number : <numbF> | <numbI> ;                  \
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%^]+/ ; \
      string : /\"(\\\\.|[^\"])*\"/ ;               \
      comment : /;[^\\r\\n]*/ ;                     \
      sexpr  : '(' <expr>* ')' ;                    \
      qexpr  : '{' <expr>* '}' ;                    \
      expr   : <number>  | <symbol> | <string> |    \
               <comment> | <sexpr>  | <qexpr> ;     \
      lispy  : /^/ <expr>* /$/ ;                    \
        ",
        Number, NumbI, NumbF, Symbol, Comment, String, 
        Sexpr, Qexpr, Expr, Lispy);

    printf("Lispy Version %x (build %x m.%d)\n", lisp_version, lisp_build, 
#ifndef _DEBUG
        mv
#else
        0
#endif
    );

    rootpack = pack_init(); /* create the root elem for packages */
    
    lenv* e = lenv_new();
    
    lenv_add_builtins(e);

    if (argc == 1) {
        puts("Press Ctrl+c to Exit\n");
        while (1) {

            char* input = readline("lispy> ");
            add_history(input);

            mpc_result_t r;
            if (mpc_parse("<stdin>", input, Lispy, &r)) {
                lval* x = lval_eval(e, lval_read(r.output));
                lval_println(x);
                lval_del(x);
                mpc_ast_delete(r.output);
            }
            else {
                mpc_err_print(r.error);
                mpc_err_delete(r.error);
            }

            free(input);
        }
    }
    
    /* Supplied with list of files */
    if (argc >= 2) {

        /* loop over each supplied filename (starting from 1) */
        for (int i = 1; i < argc; i++) {

            /* Argument list with a single argument, the filename */
            lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));

            /* Pass to builtin load and get the result */
            lval* x = builtin_load(e, args);

            /* If the result is an error be sure to print it */
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }
    }

    lenv_del(e);

    mpc_cleanup(10, Number, NumbI, NumbF, Symbol, String, Comment, 
        Sexpr, Qexpr, Expr, Lispy);

    return 0;
}