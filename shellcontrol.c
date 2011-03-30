#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <linux/errno.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "builtins.h"
#include "command.h"
#include "jobcontrol.h"
#include "shellcontrol.h"

int sh_get_stdin(Command *cmd);
int sh_get_stdout(Command *cmd);
int sh_run_cmd(Command *cmd, int inp, int outp, int no_pipe, pid_t gid);
int sh_is_executable(Command *c);
int sh_is_pipeline_valid(Pipeline *ppl);

int sh_init() {
  sigset_t mask;
  struct sigaction act;

  /* Initialize the jobs table */
  memset(&sh_jobs, 0, sizeof(sh_jobs));
  sh_jobs.fg_job = JT_NO_FG_JOB;

  /* Create a new session */
  setsid();

  /* Create a new process group */
  setpgid(getpid(), getpid());

  /* Acquire tty control */
  tcsetpgrp(STDIN_FILENO, getpgrp());

  /* Setup the SIGCHLD handler */
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = &sh_chld_handler;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGCHLD, &act, NULL);

  /* Ignore SIGTTIN, STTOU and SIGTSTP handler */
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = (void*) SIG_IGN;
  sigaction(SIGTTIN, &act, NULL);
  sigaction(SIGTTOU, &act, NULL);
  sigaction(SIGTSTP, &act, NULL);

  /* Setup the signal mask */
  sigemptyset(&mask);
  sigaddset(&mask, SIGTSTP | SIGTTOU | SIGTTIN | SIGCHLD);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  return 1;
}


int sh_process_pipeline(Pipeline* ppl) {
  int i;
  int error = 0;
  int inp = 0;
  int outp = 0;
  int cmd_count = pipe_cmdcount(ppl);
  int jid;
  Command** cmds = ppl->cmds;

  sh_update_jobs();

  if(!sh_is_pipeline_valid(ppl))
    return -1;
    
  /*Try to execute a shell builtin*/
  switch(sh_exec_cmd(ppl->cmds[0])) {
    case ECMD_EXIT:
      return 0;
    case ECMD_SUCCESS:
      return 1;
  }

  jid = sh_add_job(ppl);
  if(jid == -1) {
    fprintf(stderr, "ERROR: All job slots occupied.\n");
    return -1;
  }

  for(i = cmd_count - 1; i >= 0; i--) {
    /* Setup input */
    if(i == 0)
      inp = sh_get_stdin(cmds[i]);
    if(inp < 0) { 
      fprintf(stderr, "ERROR: Unable to open %s for reading\n", cmds[i]->input);
      error = 1;
      break;
    }

    /* Setup output */
    if(cmds[i+1] == NULL) {
      outp = sh_get_stdout(cmds[i]);

      if(outp < 0) {
        fprintf(stderr, "ERROR: Unable to open %s for writing\n",
            cmds[i]->input);
        error = 1;
        break;
      }
    }

    /* Execute command */
    outp = sh_run_cmd(cmds[i], inp, outp, i == 0, ppl->gid);
    if(outp < 0) {
      fprintf(stderr, "ERROR: Unable to create the pipeline\n");
      break;
    }

    /* Last process determines the gid */
    if(i == cmd_count - 1) {
      ppl->gid = cmds[i]->pid;
    }
  }

  /*If an error occurs, kill all of its processes and destroy the pipeline*/
  if(error) {
    sh_jobs.jobs[jid].js = SH_UNUSED;
    fprintf(stderr, "Error ocurred?\n");
    if(ppl->gid != 0)
      kill(-ppl->gid, SIGKILL);
    destroy_pipeline(ppl);
  } 

  if(!ppl->bg) {
    sh_foreground_job(jid);
  }

  return 1;
}

int sh_get_stdin(Command *cmd) {
  if(cmd->input) {
    return open(cmd->input, O_RDONLY);
  }
  return STDIN_FILENO;
}

int sh_get_stdout(Command *cmd) {
  if(cmd->output) {
    return open(cmd->output, O_WRONLY | O_CREAT);
  }
  return STDOUT_FILENO;
}
  
int sh_run_cmd(Command *cmd, int inp, int outp, int no_pipe, pid_t gid) {
  int pid;
  int pfd[2];

  /*Input specified ?*/
  if(!no_pipe) {
    if (pipe(pfd) == -1)
      return -1;

    inp = pfd[0];
  }
  
  pid = vfork();
  if(!pid) { /*Child*/
    /*Change group id*/
    if(gid)
      setpgid(getpid(), gid);
    else
      setpgid(getpid(), getpid());

    /*Close pipes (if necessary)*/
    if(inp != STDIN_FILENO) {
      dup2(inp, STDIN_FILENO);
      close(inp);
    }
    
    if(outp != STDOUT_FILENO) {
      dup2(outp, STDOUT_FILENO);
      close(outp);
    }
    
    if(!no_pipe) 
      close(pfd[1]);

    /*Change SIGTSTP handler to SIG_DFL instead of SIG_IGN*/
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = (void*) SIG_DFL;
    sigaction(SIGTSTP, &act, NULL);

    /*Exec the program*/
    execvp(cmd->cmd, cmd->args);
    fprintf(stderr, "ERROR: Unable to run: %s\n", cmd->cmd);
    exit(-1);
    return 0;
  } else { /*Parent*/
    cmd->pid = pid;

    if(outp != STDOUT_FILENO)
      close(outp);

    if(!no_pipe) {
      close(pfd[0]);
      return pfd[1];
    }

    return 0;
  }
}

int sh_is_executable(Command *c) {
  char buffer[1024];
  char *path, *next;
  int  len;
  char* cmd = c->cmd;

  /*If it's an absolute or relative path, just check if it's executable*/
  if(strchr(cmd, '/')) {
    if(access(cmd, X_OK) == 0)
      return 1;
    else
      return 0;
  }

  if(!(path = getenv("PATH")))
    return 0;
  
  /*Try to execute cmd by prepending it with every path in $PATH*/
  do {
    next = strchr(path, ':');

    if(next)
      len = next - path;
    else
      len = strlen(path);

    strncpy(buffer, path, len);
    buffer[len] = '/';
    strcpy(&buffer[len + 1], cmd);

    if(access(buffer, X_OK) == 0)
      return 1;

    path = next + 1;
  } while (next);

  return 0;
}

int sh_is_pipeline_valid(Pipeline *ppl) {
  Command **c;

  for(c = ppl->cmds; *c != NULL; c++) {
    /*If the first command is a builtin, the pipeline is valid*/
    if (c == ppl->cmds && sh_is_builtin(*c))
      return 1;

    if (!sh_is_executable(*c)) {
      fprintf(stderr, "ERROR: Invalid command: \"%s\"\n", (*c)->cmd);
      return 0;
    }
  }

  return 1;
}
