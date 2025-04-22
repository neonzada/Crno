#include "mpc.h"

#define BUFSIZE 2048

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
typedef struct{
  int type;
  union{
    long num;
    int err;
  };
} lval;

enum lval_types { LVAL_NUM, LVAL_ERR };
enum lval_err_types { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };


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
lval lval_num(long x){
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

// constructor for lval_err
lval lval_err(int x){
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

// prints lvalues based on their type, handles errors
void lval_print(lval v){
  switch(v.type){
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    
    case LVAL_ERR:
      if(v.err == LERR_DIV_ZERO) printf("baka! division by zero");
      if(v.err == LERR_BAD_OP) printf("baka! invalid op");
      if(v.err == LERR_BAD_NUM) printf("baka! invalid num");
      break;
  }
}

void lval_println(lval v){ lval_print(v); putchar('\n'); }

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
  if(strcmp(op, "^") == 0) return lval_num((long)pow(x.num, y.num));
  return lval_err(LERR_BAD_OP);
}

// evaluates the output of a parsed AST
lval eval(mpc_ast_t* t){
  // if num is substring, just return the integer (nums have no children)
  if(strstr(t->tag, "num")){
    errno = 0; //check for errors to ensure conrrect conversion
    long x = strtol(t->contents, NULL, 10);
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

int main(int argc, char** argv){
  // grammar definition
  mpc_parser_t* Num = mpc_new("num");
  mpc_parser_t* Op = mpc_new("op");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Crno = mpc_new("crno");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                             \
      num  : /-?[0-9]+/ ;                         \
      op   : '+' | '-' | '*' | '/' | '%' | '^' ;  \
      expr : <num> | '(' <op> <expr>+ ')' ;       \
      crno : /^/ <op> <expr>+ /$/ ;               \
    ",
    Num, Op, Expr, Crno);

  // interactive prompt
  printf("Crno 9.9.9\nCTRL + C to quit\n");
  while(1){
    char* input = readline("crno> ");
    add_history(input);

    // parsing user input
    mpc_result_t r;
    if(mpc_parse("<stdin>", input, Crno, &r)){
      mpc_ast_print(r.output);
      printf("Number of nodes: %d\n", count_nodes(r.output));
      lval res = eval(r.output);
      lval_println(res);
      mpc_ast_delete(r.output);
    }else{
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  // clean up the parsers
  mpc_cleanup(4, Num, Op, Expr, Crno);
  return 0;
}