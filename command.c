#include <stdlib.h>
#include <memory.h>
#include "util.h"
#include "command.h"

Command* new_command() {
  Command* cmd = (Command*) malloc(sizeof(struct Command));
  memset(cmd, 0, sizeof(*cmd));

  return cmd;
}

void destroy_command(Command* cmd) {
  int i;

  safe_free(cmd->cmd);
  safe_free(cmd->input);
  safe_free(cmd->output);

  for(i = 1; cmd->args[i] != NULL; i++) {
    free(cmd->args[i]);
  }

  free(cmd);
}

int cmd_argcount(Command* cmd) {
  int i;
  for(i = 0; i < SH_ARG_MAX && cmd->args[i] != NULL; i++);

  return i;
}

int cmd_addarg(Command* cmd, char* arg) {
  int pos = cmd_argcount(cmd);

  if(pos >= SH_ARG_MAX)
    return 0;

  cmd->args[pos] = arg;

  return 1;
}

Pipeline* new_pipeline() {
  Pipeline* ppl = (Pipeline*) malloc(sizeof(struct Pipeline));
  memset(ppl, 0, sizeof(*ppl));

  return ppl;
}

void destroy_pipeline(Pipeline* ppl) {
  int i;
  for(i = 0; ppl->cmds[i] != NULL; i++) {
    destroy_command(ppl->cmds[i]);
  }

  safe_free(ppl->cmdline);
  free(ppl);
}

int pipe_cmdcount(Pipeline* ppl) {
  int i;
  for(i = 0; i < SH_PIPELINE_MAX && ppl->cmds[i] != NULL; i++);

  return i;
}

int pipe_addcmd(Pipeline* ppl, Command* cmd) {
  int pos = pipe_cmdcount(ppl);

  if(pos >= SH_PIPELINE_MAX)
    return 0;

  ppl->cmds[pos] = cmd;

  return 1;
}
