#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "command.h"
#include "shellcontrol.h"

/*Warning suppression*/
extern int   yylex_destroy();
extern int   yyparse(void *);
extern void* yy_scan_string(char *);

/**
* Returns the prompt string as user@host:curdir$
*
* @param dest String to receive the prompt
*/
void sh_get_prompt(char *dest);

int main() {
  char prompt[2048], *input;
  Pipeline* pipeline;
  int done = 0;

  /*Enables autocomplete*/
  rl_bind_key('\t',rl_complete);

  sh_init();
  while (!done) {
    sh_get_prompt(prompt);
    input=readline(prompt);

    /*EOF*/
    if(!input)
      break;

    add_history(input);

    yy_scan_string(input);
    if (!yyparse(&pipeline) && pipeline) {
      pipeline->cmdline = input;
      done = !sh_process_pipeline(pipeline);
    }
    yylex_destroy();
  }
  return 0;
}

int yyerror(void *x, const char*msg) {
  printf("ERROR: %s\n", msg);
  return 0;
}

void sh_get_prompt(char *dest) {
  char hostname[1024];
  char cwd[1024];

  getcwd(cwd,1024);
  gethostname(hostname, 1024);

  sprintf(dest,"%s@%s:%s$ ",
    getenv("USER"),
    hostname,
    cwd);
}
