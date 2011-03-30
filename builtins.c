#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "jobcontrol.h"
#include "builtins.h"

BuiltinCmd sh_builtins[] = {
  { "exit", sh_cmd_exit },
  { "cd", sh_cmd_cd },
  { "jobs", sh_cmd_jobs },
  { "kill", sh_cmd_kill },
  { "fg", sh_cmd_fg },
  { "bg", sh_cmd_bg },
  { "history", NULL },
  { NULL, NULL }
};


int sh_cmd_fg(Command* cmd) {
  int jid;
  Job* job;

  if(cmd->args[1] && sscanf(cmd->args[1], "%%%d", &jid) > 0) {
    if(jid > 0 && jid < SH_MAX_JOBS) {
      jid -= 1;
      job = &sh_jobs.jobs[jid];
      if(job->js != SH_UNUSED && job->js != SH_TERMINATED) {
        sh_foreground_job(jid);
      }
    }
  }
  return ECMD_SUCCESS;
}

int sh_cmd_bg(Command* cmd) {
  int jid;
  Job* job;

  if(cmd->args[1] && sscanf(cmd->args[1], "%%%d", &jid) > 0) {
    if(jid > 0 && jid < SH_MAX_JOBS) {
      jid -= 1;
      job = &sh_jobs.jobs[jid];
      if(job->js != SH_UNUSED && job->js != SH_TERMINATED) {
        kill(-job->ppl->gid, SIGCONT);
      }
    }
  }
  return ECMD_SUCCESS;
}

int sh_cmd_jobs(Command* cmd) {
  int i;
  for(i = 0; i < SH_MAX_JOBS; i++) {
    if(sh_jobs.jobs[i].js != SH_UNUSED)
      sh_print_job(i);
  }

  return ECMD_SUCCESS;
}

int sh_cmd_cd(Command* cmd) {
  int fd;
  if(!cmd->args[1]) {
    fd = open("~", O_RDONLY);
  } else {
    fd = open(cmd->args[1], O_RDONLY);
  }
  if(fchdir(fd) != 0) {
    fprintf(stderr, "ERROR: Unable to change directory.\n");
  }

  return ECMD_SUCCESS;
}

int sh_cmd_exit(Command* cmd) {
  return ECMD_EXIT;
}

int sh_cmd_kill(Command* cmd) {
  int i = 0;
  int tmp;
  for(i = 1; cmd->args[i] != NULL; i++) {
    char* arg = cmd->args[i];
    /* Kill shell job */
    if(arg[0] == '%') { 
      if(sscanf(arg+1, "%d", &tmp) > 0) {
        if(tmp > 0 && tmp < SH_MAX_JOBS) { /* Check bounds */
          if(sh_jobs.jobs[tmp-1].js != SH_UNUSED) {
            kill(-sh_jobs.jobs[tmp-1].ppl->gid, SIGKILL);
          }
        }
      }
    /* Kill process */
    } if(sscanf(arg, "%d", &tmp) > 0) {
      if(tmp > 0)
        kill(tmp, SIGKILL);
    }
  }
  return ECMD_SUCCESS;
}

int sh_exec_cmd(Command *cmd) {
  int i;
  int res = ECMD_FAILED;
  for(i = 0; sh_builtins[i].cmd != NULL; i++) {
    if(!strcmp(cmd->cmd, sh_builtins[i].cmd)) {
      res = sh_builtins[i].fn(cmd);
      break;
    }
  }
  return res;
}

int sh_is_builtin(Command *cmd) {
  int i;
  int res = ECMD_FAILED;
  for(i = 0; sh_builtins[i].cmd != NULL; i++) {
    if(!strcmp(cmd->cmd, sh_builtins[i].cmd)) {
      return 1;
    }
  }
  return 0;
}
