#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include "mpc.h"

/* Builtins */
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define LASSERT(arg, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(arg); \
    return err; \
  }
#define LASSERT_TYPE(func, args, index, expected) \
  LASSERT(args, args->cell[index]->type == expected, \
    "Function %s passed incorrect type for argument %i \n Got %s Expected %s" \
    , func, index, ltype_name(args->cell[index]->type), ltype_name(expected))

#define LASSERT_NUM(func, args, num)\
  LASSERT(args, args->count == num, "Function %s passed incorrect number of arguments, \nGot %i Expected %i.", func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, "Function %s passed {} for argument %i", func, index)


#ifdef _WIN32


static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt){
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy,buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake History Function */
void add_history(char* unused){}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef struct lval{
  int type;
  long num;
  char* err;
  char *sym;
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;

  int count;
  struct lval** cell;
} lval;

struct lenv {
  lenv* par;
  int count;
  char** syms;
  lval** vals;
};

void lenv_del(lenv* e);
lval* lval_copy(lval* v);
void lenv_del(lenv* e);
lval* lval_copy(lval* v);
lval* lval_call(lenv* e, lval* f, lval* a);

char* ltype_name(int t) {
  switch(t){
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

/* Lval Operation Function */

lval* lval_num(long x){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = malloc(512);

  vsnprintf( v->err, 511, fmt, va);

  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);
  return v;
}

lval* lval_sym(char* s){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s)+1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

void lenv_del(lenv* e);


void lval_del(lval* v) {
    switch(v->type){

    case LVAL_NUM: break;

    case LVAL_SYM:
      free(v->sym); break;
    case LVAL_ERR:
      free(v->err); break;

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i=0; i<v->count; i++) {
        lval_del(v->cell[i]);
      }
      free(v->cell);
    break;

    case LVAL_FUN: 
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
    break;
  }
  free(v);
}


lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval* lval_read_num(mpc_ast_t* t){
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t){
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* x=NULL;
  if (strcmp(t->tag,">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag,"sexpr")) { x = lval_sexpr(); }
  if (strstr(t->tag,"qexpr")) { x = lval_qexpr(); }

  for (int i = 0; i<t->children_num; i++) {
    if (strcmp(t->children[i]->contents,"(") == 0) { continue; }
    if (strcmp(t->children[i]->contents,")") == 0) { continue; }
    if (strcmp(t->children[i]->contents,"{") == 0) { continue; }
    if (strcmp(t->children[i]->contents,"}") == 0) { continue; }
    if (strcmp(t->children[i]->tag,"regex") == 0) { continue; }
    x = lval_add(x,lval_read(t->children[i]));
  }
  return x;
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));

  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);

  for (int i = 0; i<e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i] + 1));
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    case LVAL_FUN: 
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
      break;
    case LVAL_NUM: x->num = v->num; break;

    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count =  v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i=0; i<x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }

  return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i=0 ; i<v->count; i++) {
    lval_print(v->cell[i]);

    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v){
  switch (v->type) {
    case LVAL_NUM: printf( "%li", v->num ); break;
    case LVAL_ERR: printf( "Error: %s", v->err); break;
    case LVAL_SYM: printf( "%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    case LVAL_FUN: 
      if (v->builtin) {
        printf("<builtin>");
      } else {
        printf("(\\ "); lval_print(v->formals);
        putchar(' '); lval_print(v->body); putchar(')');
      }
      break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  v->count--;

  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* lenv functions */
lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i=0; i<e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }

  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {
  for (int i=0; i<e->count; i++) {
    if(strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  if (e->par) {
    return lenv_get(e->par, k);
  }
  return lval_err("unbound symbol %s!", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
  for (int i=0; i<e->count; i++) {
    if(strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count-1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  while (e->par)
  {
     e = e->par;
  }
  lenv_put(e, k, v);
}

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  v->builtin = NULL;

  v->env = lenv_new();

  v->formals = formals;
  v->body = body;
  return v;
}

lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT_NUM("\\", a,2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  for (int i = 0; i<a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM), "Cannot define non symbol. Got %s, Expected %s",ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);

  lval_del(a);
  return lval_lambda(formals, body);
}

lval* builtin_op(lenv* e, lval* a, char* op) {
  for (int i = 0; i<a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non numerical values!");
    }
  }

  lval* x = lval_pop(a,0);
  if(strcmp(op,"-") == 0 && a->count == 0) { x->num = -x->num; }

  while (a->count > 0) {
    lval* y = lval_pop(a,0);

    if(strcmp(op, "+") == 0 ) { x->num += y->num; }
    if(strcmp(op, "-") == 0 ) { x->num -= y->num; }
    if(strcmp(op, "*") == 0 ) { x->num *= y->num; }
    if(strcmp(op, "/") == 0 ) {
      if(y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division by Zero"); break;
      }
      x->num /= y->num;
    }

    if(strcmp(op, "%") == 0 ) { x->num %= y->num; }
    if(strcmp(op, "min") == 0 ) { x->num = min(x->num, y->num); }
    if(strcmp(op, "max") == 0 ) { x->num = max(x->num, y->num); }
    if(strcmp(op, "^") == 0 ) { x->num = pow(x->num, y->num); }
    lval_del(y);
  }
  lval_del(a); return x;
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

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "%");
}

lval* builtin_min(lenv* e, lval* a) {
    return builtin_op(e, a, "min");
}

lval* builtin_max(lenv* e, lval* a) {
    return builtin_op(e, a, "max");
}

lval* builtin_pow(lenv* e, lval* a) {
    return builtin_op(e, a, "^");
}


lval* builtin_ord(lenv* e, lval* a, char op) {
  LASSERT_NUM( op, a, 2);
  LASSERT_TYPE( op, a, 0, LVAL_NUM);
  LASSERT_TYPE( op, a, 1, LVAL_NUM);

  int r;
  
  if (strcmp(op, ">") == 0) { r = a->cell[0]->num > a->cell[1]->num; }
  if (strcmp(op, ">=") == 0) { r = a->cell[0]->num >= a->cell[1]->num; }
  if (strcmp(op, "<") == 0) { r = a->cell[0]->num < a->cell[1]->num; }
  if (strcmp(op, "<=") == 0) { r = a->cell[0]->num <= a->cell[1]->num; }

  return lval_num(r);
}

lval* builtin_gt(lenv* e, lval* a) {
  return builtin_ord(e, a, ">");
}

lval* builtin_gteq(lenv* e, lval* a) {
  return builtin_ord(e, a, ">=");
}

lval* builtin_ls(lenv* e, lval* a) {
  return builtin_ord(e, a, "<");
}

lval* builtin_lseq(lenv* e, lval* a) {
  return builtin_ord(e, a, "<=");
}

lval* lval_eval_sexpr( lenv* e, lval* v );

lval* lval_eval(lenv* e, lval* v){
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  return v;
}

int lval_cmp(lval* a, lval* b){
  if (a->type != b->type) { return 0; }

  if (a->type == LVAL_NUM) { return a->num == b->num; }
  if (a->type == LVAL_ERR) { return strcmp(a->err,b->err); }
  if (a->type == LVAL_SYM) { return strcmp(a->sym,b->sym); }

  if (a->type == LVAL_FUN) {
    if (a->builtin || b->builtin) { return a->builtin == b->builtin; }
    else { return lval_cmp(a->formals, b->formals) || lval_cmp(a->body, b->body); }
  }

  if (a->type == LVAL_QEXPR || a->type == LVAL_SEXPR) {
    if (!(a->count == b->count)) { return 0; }
    for (int i = 0; i < a->count; i++) {
      if (!lval_cmp(a->cell[i], b->cell[i])) { return 0; }
    }
    return 1;
  }
}


lval* builtin_cmp(lenv* e, lval* a, char* op) {
  LASSERT_NUM(op, a, 2);

  int r;
  if (strcmp(op, "==")==0) {
    r = lval_cmp(a->cell[0], a->cell[1]);
  }else if (strcmp(op, "!=") == 0) {
    r = !lval_cmp(a->cell[0], a->cell[1]);
  }

  return lval_num(r);
}

lval* builtin_head(lenv* e, lval* a) {
  LASSERT(a, a->count == 1, "Function head passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function head passed incorrect type for argument 0 \n Got %s expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  LASSERT(a, a->cell[0]->count != 0, "Function head passed {}!");

  lval* v = lval_take(a,0);
  while(v->count > 1 ) { lval_del(lval_pop(v,1)); }
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT(a, a->count == 1, "Function tail passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function tail passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0, "Function tail passed {}!");

  lval* v = lval_take(a,0);
  lval_del(lval_pop(v,1));
  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a){
  LASSERT(a, a->count == 1, "Function eval passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function eval passed incorrect type!");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y){
  while(y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i<a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function join passed incorrect type!");
  }
  lval* x = lval_pop(a, 0);

  while (a->count){
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}



lval* lval_eval_sexpr( lenv* e, lval* v ) {
  for (int i=0; i<v->count; i++){
    v->cell[i] = lval_eval(e, v->cell[i]);
  }
  for (int i=0; i<v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v,i);
    }
  }

  if (v->count == 0) { return v; }

  if (v->count == 1) { return lval_take(v,0); }

  lval* f = lval_pop(v,0);
  if (f->type != LVAL_FUN) {
    lval* err = lval_err("S-Expression startss with incorrect type \n Got %s Expected %s", ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(v); lval_del(f);
    return err;
  }

  lval* result = lval_call(e, f, v);
  lval_del(f);
  return result;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);

    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}


lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  lval* sym = a->cell[0];
  for (int i = 0; i<sym->count; i++) {
    LASSERT(a, (sym->cell[i]->type == LVAL_SYM), "Function %s cannot define non symbol. \n Got %s, expected %s.", func, ltype_name(sym->cell[i]->type), ltype_name(LVAL_SYM));
  }

  LASSERT(a, (sym->count == a->count - 1), "Function %s passed too many arguments for symbol. \n Got %i Expected %i", func, sym->count, a->count-1);

  for (int i=0; i < sym->count; i++) {
    if(strcmp(func, "def") == 0) {
      lenv_def(e, sym->cell[i], a->cell[i+1]);
    } 

    if(strcmp(func, "=") == 0) {
      lenv_put(e, sym->cell[i], a->cell[i+1]);
    }
  }

  lval_del(a);
  return lval_sexpr();
}

lval* lval_call(lenv* e, lval* f, lval* a) {
  if (f->builtin) { return f->builtin(e, a);}

  int given = a->count;
  int total = f->formals->count;

  while(a->count) { 
    if (f->formals->count == 0) {
      lval_del(a); return lval_err("Function passed too many arguments \n Got %i expected %i ", given, total);
    }
    
    lval* sym = lval_pop(f->formals, 0);

    lval* val = lval_pop(a, 0);

    lenv_put(f->env, sym, val);

    lval_del(sym); lval_del(val);
  }

  lval_del(a);

  if (f->formals->count == 0) {
    f->env->par = e;
    
    return builtin_eval(
      f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    return lval_copy(f);
  }
  
}

lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

void lenv_add_builtins(lenv* e) {
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
  
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "=", builtin_put);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "min", builtin_min);
    lenv_add_builtin(e, "max", builtin_max);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);

    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, ">=", builtin_gteq);
    lenv_add_builtin(e, "<", builtin_ls);
    lenv_add_builtin(e, "<=", builtin_lseq);

    lenv_add_builtin(e, "==", builtin_cmp);
    lenv_add_builtin(e, "!=", builtin_cmp);

}



int main(int argc, char** argv){
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                  \
    number   : /-?[0-9]+/ ;                            \
    symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%^]+/ ;         \
    sexpr    : '(' <expr>* ')' ;                       \
    qexpr    : '{' <expr>* '}' ;                       \
    expr     : <number> | <symbol> | <sexpr> | <qexpr> ;         \
    lispy    : /^/ <expr>* /$/ ;            \
    ",
     Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("LISPY V-0.0.1");
  puts("Crtl+C to exit");

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  while (1){
    char* input = readline("lispy> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)){
      lval* result = lval_eval(e, lval_read(r.output));
      lval_println(result);
      lval_del(result);
      mpc_ast_delete(r.output);
    }else{
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Qexpr, Sexpr, Expr, Lispy);
  return 0;
}
