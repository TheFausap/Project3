#include <stdio.h>

#define	MAXDIGITS	100		/* maximum length bignum */ 

#define PLUS		1		/* positive sign bit */
#define MINUS		-1		/* negative sign bit */

typedef struct {
    char digits[MAXDIGITS];         /* represent the number */
    int signbit;			/* 1 if positive, -1 if negative */
    int lastdigit;			/* index of high-order digit */
} bignum;

void print_bignum(bignum* n);
void int_to_bignum(intptr_t s, bignum* n);
void initialize_bignum(bignum* n);
void add_bignum(bignum* a, bignum* b, bignum* c);
void subtract_bignum(bignum* a, bignum* b, bignum* c);
int compare_bignum(bignum* a, bignum* b);
void multiply_bignum(bignum* a, bignum* b, bignum* c);
void divide_bignum(bignum* a, bignum* b, bignum* c);
