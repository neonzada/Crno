#include "mpc.h"

#define BUFSIZE 2048
#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

#ifdef _WIN32

static char buffer[BUFSIZE];

// simulates readline.h function for compatibility purposes
char* readline(char* prompt){
  fputs(prompt, stdout);
  fgets(buffer, BUFSIZE, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

// for compatibility purposes, does nothing
void add_history(char* unused){}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

// lisp value
typedef struct lval{
  int type;
  union{
    double num;
    char* err;
  };
  char* sym;
  int count;
  struct lval** cell;
} lval;

// ----- forward declarations -----

int count_nodes(mpc_ast_t* t);

lval* lval_num(double x);
lval* lval_err(char* m);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
void lval_del(lval* v);

lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);
lval* lval_add(lval* v, lval* x);

void lval_expr_print(lval* v, char open, char close);
void lval_print(lval* v);
void lval_println(lval* v);

lval* lval_eval_sexpr(lval* v);
lval* lval_eval(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);

// builtin funcs
lval* builtin(lval* a, char* func);
lval* builtin_op(lval* a, char* op);
lval* builtin_head(lval* a);
lval* builtin_tail(lval* a);
lval* builtin_list(lval* a);
lval* builtin_eval(lval* a);
lval* builtin_join(lval* a);
//lval eval_op(lval x, char* op, lval y);
//lval eval(mpc_ast_t* t);

enum lval_types { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };
enum lval_err_types { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

int main(int argc, char** argv){
  // grammar definition
  mpc_parser_t* Num = mpc_new("num");
  mpc_parser_t* Sym = mpc_new("sym");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Crno = mpc_new("crno");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                              \
      num   : /-?[0-9]*\.?[0-9]+/ ;                \
      sym   : '+' | '-' | '*' | '/' | '%' | '^'    \
            | \"list\" | \"head\" | \"tail\"       \
            | \"join\" | \"eval\" ;                \
      sexpr : '(' <expr>* ')' ;                    \
      qexpr : '{' <expr>* '}' ;                    \
      expr  : <num> | <sym> | <sexpr> | <qexpr> ;  \
      crno  : /^/ <expr>* /$/ ;                    \
    ",
    Num, Sym, Sexpr, Qexpr, Expr, Crno);

  // interactive prompt
  printf("Crno v9.9.9\nCTRL + C to quit\n");
  while(1){
    char* input = readline("crno> ");
    add_history(input);

    // parsing user input
    mpc_result_t r;
    if(mpc_parse("<stdin>", input, Crno, &r)){
      // mpc_ast_print(r.output);
      // printf("Number of nodes: %d\n", count_nodes(r.output));

      // lval res = eval(r.output);
      // lval_println(res);

      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);

      mpc_ast_delete(r.output);
    }else{
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  // clean up the parsers
  mpc_cleanup(6, Num, Sym, Sexpr, Qexpr, Expr, Crno);
  return 0;
}

// ----- misc functions ----- //

// returns the number of nodes in an AST
int count_nodes(mpc_ast_t* t){
  if(t->children_num == 0) return 1;
  if(t->children_num >= 1){
    int count = 0;
    for(int i = 0; i < t->children_num; i++){
      count += count_nodes(t->children[i]);
    }
    return count;
  }
  return 0;
}


// ----- actually important functions ----- //

// constructor for lval_num
lval* lval_num(double x){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

// constructor for lval_err
lval* lval_err(char* m){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m)+1); //ensure space for \0
  strcpy(v->err, m);
  return v;
}

// constructor for lval_sym
lval* lval_sym(char* s){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s)+1); //ensure space for \0
  strcpy(v->sym, s);
  return v;
}

// constructor for lval_sexpr
lval* lval_sexpr(void){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// constructor for lval_qexpr
lval* lval_qexpr(void){
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// destructor for lvalues
void lval_del(lval* v){
  switch(v->type){
    case LVAL_NUM: break;
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for(int i = 0; i < v->count; i++)
        lval_del(v->cell[i]);
      free(v->cell);
      break;
  }
  free(v);
}

// aux function to ensure proper strtod conversion
lval* lval_read_num(mpc_ast_t* t){
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("baka! invalid num");
}

// reads lvalues with sexpr now supported
lval* lval_read(mpc_ast_t* t){
  // num || sym -> conversion to type
  if(strstr(t->tag, "num")) return lval_read_num(t);
  if(strstr(t->tag, "sym")) return lval_sym(t->contents);

  // root || sexpr -> create empty list
  lval* x = NULL;
  if((strcmp(t->tag, ">") == 0) || (strstr(t->tag, "sexpr"))) x = lval_sexpr();

  // qexpr
  if(strstr(t->tag, "qexpr")) x = lval_qexpr();


  // fill list with expr contained
  for(int i = 0; i < t->children_num; i++){
    if(strcmp(t->children[i]->contents, "(") == 0) continue;
    if(strcmp(t->children[i]->contents, ")") == 0) continue;
    if(strcmp(t->children[i]->contents, "{") == 0) continue;
    if(strcmp(t->children[i]->contents, "}") == 0) continue;
    if(strcmp(t->children[i]->tag, "regex") == 0) continue;

    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

// aux function to add values to a list created by root or sexpr
lval* lval_add(lval* v, lval* x){
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

void lval_expr_print(lval* v, char open, char close){
  putchar(open);
  for(int i = 0; i < v->count; i++){
    lval_print(v->cell[i]);
    if(i != (v->count-1)) putchar(' '); //dont print space if last element
  }
  putchar(close);
}

// prints lvalues based on their type, the lion doesn't concern himself with error handling
void lval_print(lval* v){
  switch(v->type){
    case LVAL_NUM:   printf("%lf", v->num); break;
    case LVAL_ERR:   printf("baka! %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval* v){ lval_print(v); putchar('\n'); }

// special eval for sexpr
lval* lval_eval_sexpr(lval* v){
  for(int i = 0; i < v->count; i++)
    v->cell[i] = lval_eval(v->cell[i]);
  for(int i = 0; i < v->count; i++)
    if(v->cell[i]->type == LVAL_ERR) return lval_take(v, i);
  
  if(v->count == 0) return v; //empty expr
  if(v->count == 1) return lval_take(v, 0); //single expr

  // ensure first elem is sym
  lval* f = lval_pop(v, 0);
  if(f->type != LVAL_SYM){
    lval_del(f); lval_del(v);
    return lval_err("baka! sexpr does not start with sym!");
  }

  lval* res = builtin(v, f->sym); //builtin deals with op
  lval_del(f);
  return res;
}

lval* lval_eval(lval* v){
  return v->type == LVAL_SEXPR ? lval_eval_sexpr(v) : v;
}

lval* lval_pop(lval* v, int i){
  // get top item
  lval* x = v->cell[i];

  // shift mem and realloc
  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);

  return x;
}

lval* lval_take(lval* v, int i){
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* lval_join(lval* x, lval* y){
  while(y->count) x = lval_add(x, lval_pop(y, 0));

  lval_del(y);
  return x;
}

// ----- builtin funcs impl -----

lval* builtin(lval* a, char* func){
  if(strcmp("list", func) == 0) return builtin_list(a);
  if(strcmp("head", func) == 0) return builtin_head(a);
  if(strcmp("tail", func) == 0) return builtin_tail(a);
  if(strcmp("join", func) == 0) return builtin_join(a);
  if(strcmp("eval", func) == 0) return builtin_eval(a);

  if(strstr("+-/*%^", func)) return builtin_op(a, func);

  lval_del(a);
  return lval_err("baka! unknown fun");
}

lval* builtin_op(lval* a, char* op){
  // ensure all args are nums
  for(int i = 0; i < a->count; i++){
    if(a->cell[i]->type != LVAL_NUM){
      lval_del(a);
      return lval_err("baka! non-number");
    }
  }

  lval* x = lval_pop(a, 0);

  // no args && sub -> unary negation
  if((strcmp(op, "-") == 0) && a->count == 0) x->num = -x->num;

  while(a->count > 0){
    lval* y = lval_pop(a, 0);

    if(strcmp(op, "+") == 0) x->num += y->num;
    if(strcmp(op, "-") == 0) x->num -= y->num;
    if(strcmp(op, "*") == 0) x->num *= y->num;
    if(strcmp(op, "^") == 0) x->num = pow(x->num, y->num);
    if(strcmp(op, "%") == 0) x->num = (int)x->num % (int)y->num;
    if(strcmp(op, "/") == 0){
      if(y->num == 0){
        lval_del(x); lval_del(y);
        x = lval_err("baka! division by zero");
        break;
      }
      x->num /= y->num;
    }
    lval_del(y);
  }
  lval_del(a);
  return x;
}

lval* builtin_head(lval* a){
  LASSERT(a, a->count == 1, "baka! 'head' fun passed too many args!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "baka! 'head' fun passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0, "baka! 'head' fun passed {}!");

  lval* v = lval_take(a, 0);
  while(v->count > 1) lval_del(lval_pop(v, 1));

  return v;
}

lval* builtin_tail(lval* a){
  LASSERT(a, a->count == 1, "baka! 'tail' fun passed too many args!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "baka! 'tail' fun passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0, "baka! 'tail' fun passed {}!");

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lval* a){
  a->type = LVAL_QEXPR; // converts SExpr to QExpr, which lists it
  return a;
}

lval* builtin_eval(lval* a){
  LASSERT(a, a->count == 1, "baka! 'eval' fun passed too many args!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "baka! 'eval' fun passed incorrect type!");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* builtin_join(lval* a){
  for(int i = 0; i < a->count; i++) LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "baka! 'join' fun passed incorrect type!");

  lval* x = lval_pop(a, 0);
  while(a->count) x = lval_join(x, lval_pop(a, 0));

  lval_del(a);
  return x;
}

/*

// evaluates number operations parsed by the eval function
lval eval_op(lval x, char* op, lval y){
  if(x.type == LVAL_ERR) return x;
  if(y.type == LVAL_ERR) return y;

  if(strcmp(op, "+") == 0) return lval_num(x.num + y.num);
  if(strcmp(op, "-") == 0) return lval_num(x.num - y.num);
  if(strcmp(op, "*") == 0) return lval_num(x.num * y.num);
  if(strcmp(op, "/") == 0){
    return y.num == 0
    ? lval_err(LERR_DIV_ZERO)
    : lval_num(x.num / y.num);
  }
  if(strcmp(op, "%") == 0) return lval_num(x.num % y.num);
  if(strcmp(op, "^") == 0) return lval_num((double)pow(x.num, y.num));
  return lval_err(LERR_BAD_OP);
}

// evaluates the output of a parsed AST
lval eval(mpc_ast_t* t){
  // if num is substring, just return the integer (nums have no children)
  if(strstr(t->tag, "num")){
    errno = 0; //check for errors to ensure conrrect conversion
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  // if not num, skip ( and eval the second child
  char* op = t->children[1]->contents;
  lval x = eval(t->children[2]);

  int i = 3;
  while(strstr(t->children[i]->tag, "expr")){
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

*/