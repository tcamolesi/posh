#ifndef BUILTINS_H_DEFINED_9544576732162381
#define BUILTINS_H_DEFINED_9544576732162381

#include "command.h"

typedef int (sh_builtin_func)(Command*);
typedef struct BuiltinCmd {
  char* cmd;
  sh_builtin_func *fn;
} BuiltinCmd;

enum ExecCmdResult {
  ECMD_SUCCESS = 0,
  ECMD_FAILED,
  ECMD_EXIT
};

int sh_cmd_jobs(Command* cmd);
int sh_cmd_cd(Command* cmd);
int sh_cmd_exit(Command* cmd);
int sh_cmd_kill(Command* cmd);
int sh_cmd_fg(Command* cmd);
int sh_cmd_bg(Command* cmd);

int sh_exec_cmd(Command *cmd);


#endif
