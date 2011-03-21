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
#include "command.h"
#include "shellcontrol.h"

int sh_cmd_jobs(Command* cmd);
int sh_cmd_cd(Command* cmd);
int sh_cmd_exit(Command* cmd);
int sh_cmd_kill(Command* cmd);
int sh_cmd_fg(Command* cmd);
int sh_cmd_bg(Command* cmd);

JobTable sh_jobs;
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

int sh_get_stdin(Command *cmd);
int sh_get_stdout(Command *cmd);
int sh_run_cmd(Command *cmd, int inp, int outp, int no_pipe, pid_t gid);
int sh_init();
int sh_foreground_job(int jid);
int sh_find_job_by_pid(int pid);
int sh_exec_cmd(Command *cmd);

void sh_chld_handler(int sig, siginfo_t *siginfo, void *context);

void sh_chld_handler(int sig, siginfo_t *siginfo, void *context) {
  int pos;
  pos = sh_find_job_by_pid(siginfo->si_pid);

  /*Finish zombie processes*/
  /*if(siginfo->si_code != SH_STOPPED || siginfo->si_code != CLD_CONTINUED)*/
  waitpid(siginfo->si_pid, 0, WNOHANG);

  if(pos == -1 || sh_jobs.jobs[pos].js == SH_UNUSED)
    return;

  switch(siginfo->si_code) {
    case CLD_STOPPED:
      sh_jobs.jobs[pos].js = SH_STOPPED;
      break;

    case CLD_CONTINUED:
      sh_jobs.jobs[pos].js = SH_RUNNING;
      break;

    default:
      sh_jobs.jobs[pos].js = SH_TERMINATED;
  }
}

int sh_find_job_by_pid(int pid) {
  int i;
  Pipeline* ppl;
  for(i = 0; i < SH_MAX_JOBS; i++) {
    ppl = sh_jobs.jobs[i].ppl;
    if(ppl && ppl->gid == pid)
      return i;
  }

  return -1;
}

void sh_print_job(int jid) {
  static const char* description[] = {
    "Unused",
    "Foreground",
    "Running",
    "Stopped",
    "Terminated"
  };

  Job* job = &sh_jobs.jobs[jid];
  fprintf(stderr, "[%d]  %-25s %s\n", jid+1, description[job->js],
      job->ppl->cmdline);
}

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
  return 0;
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
  return 0;
}

int sh_cmd_jobs(Command* cmd) {
  int i;
  for(i = 0; i < SH_MAX_JOBS; i++) {
    if(sh_jobs.jobs[i].js != SH_UNUSED)
      sh_print_job(i);
  }

  return 0;
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

  return 0;
}

int sh_cmd_exit(Command* cmd) {
  return -1;
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
  return 0;
}

int sh_exec_cmd(Command *cmd) {
  int i;
  int res = 1;
  for(i = 0; sh_builtins[i].cmd != NULL; i++) {
    if(!strcmp(cmd->cmd, sh_builtins[i].cmd)) {
      res = sh_builtins[i].fn(cmd);
      break;
    }
  }
  return res;
}

void sh_update_jobs() {
  int i;
  Job* job;
  for(i = 0; i < SH_MAX_JOBS; i++) {
    job = &sh_jobs.jobs[i];
    if(job->js == SH_TERMINATED) {
      if(!job->fg)
        sh_print_job(i);
      job->js = SH_UNUSED;
      destroy_pipeline(job->ppl);
      if(i < sh_jobs.first_free)
        sh_jobs.first_free = i;
    }
  }
}

int sh_add_job(Pipeline *ppl) {
  int i, pos;
  Job* job;

  if(sh_jobs.first_free >= SH_MAX_JOBS) {
    return -1;
  }

  pos = sh_jobs.first_free;
  sh_jobs.first_free = SH_MAX_JOBS;
  for(i = pos + 1; i < SH_MAX_JOBS; i++) {
    if(sh_jobs.jobs[i].js == SH_UNUSED) {
      sh_jobs.first_free = i;
      break;
    }
  }

  job = &sh_jobs.jobs[pos];
  job->ppl = ppl;
  job->js = SH_STOPPED;

  return pos;
}

int sh_foreground_job(int jid) {
  Job* job;
  /* Check bounds*/
  if(jid < 0 || jid >= SH_MAX_JOBS)
    return -1;
  job = &sh_jobs.jobs[jid];

  /* Check if it's still active */
  if(job->js == SH_UNUSED)
    return -1;

  /* Mark as fg */
  job->fg = 1;

  /* Give tty to the process group */
  tcsetpgrp(STDIN_FILENO, job->ppl->gid);
  
  usleep(100);
  /* Wait for the pipeline to finish */
  while(1) {
    if(waitpid(-job->ppl->gid, NULL, 0) == -1) {
      if(errno == ECHILD)
        break;
    }
  }

  job->fg = 0;
/*  tcsetpgrp(STDIN_FILENO, getpgrp());
  tcsetpgrp(STDOUT_FILENO, getpgrp());*/
  return 0;
}

int sh_init() {
  struct sigaction act;

  /* Create a new session */
  setsid();

  /* Setup the SIGCHLD handler */
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = &sh_chld_handler;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGCHLD, &act, NULL);

  /* Initialize the jobs table */
  memset(&sh_jobs, 0, sizeof(sh_jobs));

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
    case -1:
      return 0;
    case 0:
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
