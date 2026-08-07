#include "mpc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

static char buffer[2048];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>

#endif
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct lval {
  int type;
  long num;
  char *err;
  char *sym;
  int count;
  struct lval **cell;
} lval;

lval *lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    free(v->cell);
    break;
  }
  free(v);
}

lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type =LVAL_ERR;
  v->err =malloc(strlen(m) + 1) ;
  strcpy(v->err, m);
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
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_read_num(mpc_ast_t* t){
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid numver");
}
lval* lval_add(lval* v,lval* x){
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*)* v->count);
  v->cell[v->count-1 ] = x;
  return v;
}

lval* lval_read(mpc_ast_t* t){
  if(strstr(t->tag, "number")) return lval_read_num(t);
  if(strstr(t->tag, "symbol")) return lval_sym(t->contents);
  lval* x = NULL;
  if(strcmp(t->tag, ">") == 0) x= lval_sexpr();
  if(strcmp(t->tag, "sexpr") == 0) x = lval_sexpr();
  for(int i = 0; i < t->children_num; i++){
    if(strcmp(t->children[i]->contents, "(") == 0) continue;
    if(strcmp(t->children[i]->contents, ")") == 0) continue;
    if(strcmp(t->children[i]->tag, "regex") == 0) continue;
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    /* Print Value contained within */
    lval_print(v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }


lval* lval_eval(lval* v);

lval* lval_pop(lval* v, int i) {
  /* Find the item at "i" */
  lval* x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1],
    sizeof(lval*) * (v->count-i-1));

  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i){
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* a, char* op){
for(int i = 0; i < a->count; i++){
  if(a->cell[i] -> type != LVAL_NUM){
    lval_del(a);
    return lval_err("fuck you man  you cannot operate on non-number");
  }
}
lval* x = lval_pop(a, 0);
if((strcmp(op,"-") == 0) && a->count == 0) x->num = -x ->num;
while(a->count > 0){
  lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero!"); break;
      }
      x->num /= y->num;
    }
    lval_del(y);
}
lval_del(a); return x;
}




lval* lval_eval_sexpr(lval* v){
  for(int i = 0; i < v->count; i++){
    v->cell[i] = lval_eval(v->cell[i]);
  }
  for(int i = 0; i< v-> count; i++){
    if(v->cell[i]->type == LVAL_ERR) return lval_take(v,i);
  }
  if(v->count == 0) return v;
  if(v->count == 1) return lval_take(v, 0);
  lval* f = lval_pop(v, 0);
  if(f-> type != LVAL_SYM){
    lval_del(f);lval_del(v);
    return lval_err("S-expr Dose Not start with symbol son of bitch!!!!");
  }
  lval* result = builtin_op(v, f->sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v){
  if(v->type == LVAL_SEXPR) return lval_eval_sexpr(v);
  return v;
};
// lval eval_op(lval x, char *op, lval y) {
//   if (x.type == LVAL_ERR)
//     return x;
//   if (y.type == LVAL_ERR)
//     return y;

//   if (strcmp(op, "+") == 0) {
//     return lval_num(x.num + y.num);
//   }
//   if (strcmp(op, "-") == 0) {
//     return lval_num(x.num - y.num);
//   }
//   if (strcmp(op, "*") == 0) {
//     return lval_num(x.num * y.num);
//   }
//   if (strcmp(op, "/") == 0) {

//     return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
//   }
//   return lval_err(LERR_BAD_OP);
// }

// lval eval(mpc_ast_t *t) {
//   if (strstr(t->tag, "number")) {
//     errno = 0;
//     long x = strtol(t->contents, NULL, 10);
//     return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
//   }
//   char *op = t->children[1]->contents;
//   lval x = eval(t->children[2]);
//   int i = 3;
//   while (strstr(t->children[i]->tag, "expr")) {
//     x = eval_op(x, op, eval(t->children[i]));
//     i++;
//   }
//   return x;
// }

int main(int argc, char **argv) {

  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                     \
      number   : /-?[0-9]+/ ;                             \
      symbol : '+' | '-' | '*' | '/' ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
            Number, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

  while (1) {

    char *input = readline("lispy> ");
    add_history(input);

  //   /* Attempt to parse the user input */
     mpc_result_t r;

if(mpc_parse("", input, Lispy, &r))
{
    lval* x = lval_read(r.output);

    lval_println(x);

    lval_del(x);

    mpc_ast_delete(r.output);
}
else
{
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
}

free(input);
}
  return 0;
}
