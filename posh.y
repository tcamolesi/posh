%{
#include <stdio.h>

extern int yylex(void);
%}

%parse-param {Pipeline** result}

%code requires {
#include "command.h"
}

%union {
  Pipeline *pipeline;
  Command  *command;
  char     *str;
}

%token STR
%type<pipeline> input executable_expression pipeline
%type<command> command
%type<str> out_red in_red STR

%%

input
  : /**empty*/ { 
      *result = NULL;
    }
  | executable_expression {
      *result = $1;
    }

executable_expression
  : pipeline {
      $$ = $1;
    }
  | pipeline '&' {
      $$ = $1;
      $$->bg = 1;
    }

pipeline
  : command {
      $$ = new_pipeline();
      pipe_addcmd($$, $1);
    }
  | pipeline '|' command {
      $$ = $1;
      pipe_addcmd($$, $3);
    }

command
  : STR {
      $$ = new_command();
      cmd_addarg($$, $1);
      $$->cmd = $1;
    }
  | in_red {
      $$ = new_command();
      $$->input = $1;
    }
  | out_red {
      $$ = new_command();
      $$->output = $1;
    }
  | command STR {
      $$ = $1;
      if(!$$->cmd)
        $1->cmd = $2;
      cmd_addarg($$, $2);
    }
  | command in_red {
      $$ = $1;
      $$->input = $2;
    }
  | command out_red {
      $$ = $1;
      $$->output = $2;
    }

in_red
  : '<' STR  { $$ = $2 }

out_red
  : '>' STR  { $$ = $2 }

%%
