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
int sh_init();

void sh_chld_handler(int sig, siginfo_t *siginfo, void *context) {
  int pos;
  pos = sh_find_job_by_pid(siginfo->si_pid);

  /*Finish zombie processes*/
  waitpid(siginfo->si_pid, 0, WNOHANG);

  if(pos == -1 || sh_jobs.jobs[pos].js == SH_UNUSED)
    return;

  if (sh_jobs.jobs[pos].js == SH_TERMINATED)
    return;

  fprintf(stderr, "In child handler\n");

  switch(siginfo->si_code) {
    case CLD_STOPPED:
      sh_jobs.jobs[pos].js = SH_STOPPED;
      if( pos == sh_jobs.fg_job ) {
        sh_jobs.fg_job = JT_NO_FG_JOB;
        tcsetpgrp(STDIN_FILENO, getpgrp());
      }
      break;

    case CLD_CONTINUED:
      sh_jobs.jobs[pos].js = SH_RUNNING;
      break;

    default:
      sh_jobs.jobs[pos].js = SH_TERMINATED;
  }
}

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
  act.sa_sigaction = SIG_IGN;
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
        fprintf(stderr, "ERROR: Unable to open %s for writing\n", cmds[i]->input);
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
    if(gid)
      setpgid(getpid(), gid);
    else
      setpgid(getpid(), getpid());

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
