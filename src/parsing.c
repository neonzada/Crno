#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define BUFSIZE 2048

#ifdef _WIN32
#include <string.h>

static char buffer[BUFSIZE];

char* readline(char* prompt){
  fputs(prompt, stdout);
  fgets(buffer, BUFSIZE, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused){}

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

int main(int argc, char** argv){
  // Grammar definition
  mpc_parser_t* Num = mpc_new("num");
  mpc_parser_t* Op = mpc_new("op");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Crno = mpc_new("crno");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                       \
      num  : /-?[0-9]+/ ;                   \
      op   : '+' | '-' | '*' | '/' ;        \
      expr : <num> | '(' <op> <expr>+ ')' ; \
      crno : /^/ <op> <expr>+ /$/ ;         \
    ",
    Num, Op, Expr, Crno);

  // Interactive prompt
  printf("Crno 9.9.9\nCTRL + C to quit\n");
  while(1){
    char* input = readline("crno> ");
    add_history(input);

    // parsing user input
    mpc_result_t r;
    if(mpc_parse("<stdin>", input, Crno, &r)){
      mpc_ast_print(r.output);
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