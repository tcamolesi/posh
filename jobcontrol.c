#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "jobcontrol.h"

JobTable sh_jobs;

void sh_chld_handler(int sig, siginfo_t *siginfo, void *context) {
  int pos;
  pos = sh_find_job_by_pid(siginfo->si_pid);

  /*Finish zombie processes*/
  waitpid(siginfo->si_pid, 0, WNOHANG);

  /*Check if job is valid*/
  if(pos == -1 || sh_jobs.jobs[pos].js == SH_UNUSED)
    return;
  if (sh_jobs.jobs[pos].js == SH_TERMINATED)
    return;

  /*Update job status accordingly*/
  switch(siginfo->si_code) {
    case CLD_STOPPED:
      sh_jobs.jobs[pos].js = SH_STOPPED;

      /*If the foreground process has stopped, take control of the tty*/
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
    "Running",
    "Stopped",
    "Terminated"
  };

  Job* job = &sh_jobs.jobs[jid];
  fprintf(stderr, "[%d]  %-25s %s\n", jid+1, description[job->js],
      job->ppl->cmdline);
}

void sh_update_jobs() {
  int i;
  Job* job;
  for(i = 0; i < SH_MAX_JOBS; i++) {
    job = &sh_jobs.jobs[i];

    /*Skip unused slots*/
    if(job->js == SH_UNUSED)
      continue;

    /*Check if job has finished its execution*/
    if(waitpid(job->ppl->gid, NULL, WNOHANG) == ECHILD ||
       job->js == SH_TERMINATED) {
      if(i == sh_jobs.fg_job)
        sh_jobs.fg_job = JT_NO_FG_JOB;
      else /*Don't notify the user that a foreground job was finished*/
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

  if(sh_jobs.first_free >= SH_MAX_JOBS)
    return -1;

  /*Find a free slot*/
  pos = sh_jobs.first_free;
  sh_jobs.first_free = SH_MAX_JOBS;
  for(i = pos + 1; i < SH_MAX_JOBS; i++) {
    if(sh_jobs.jobs[i].js == SH_UNUSED) {
      sh_jobs.first_free = i;
      break;
    }
  }

  /*Setup job information*/
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
  if(job->js == SH_UNUSED || job->js == SH_TERMINATED)
    return -1;

  /* Mark it as the foreground job */
  sh_jobs.fg_job = jid;

  /* Give tty to the process group */
  tcsetpgrp(STDIN_FILENO, job->ppl->gid);

  /* Unsuspend the process */
  kill(-job->ppl->gid, SIGCONT);
  
  /* Wait for the pipeline to finish */
  while(1) {
    if(waitpid(-job->ppl->gid, NULL, 0) == -1) {
      if(errno == ECHILD)
        break;

      if(errno == EINTR && sh_jobs.fg_job == JT_NO_FG_JOB)
        break;
    }
  }

  /* Get tty back */
  tcsetpgrp(STDIN_FILENO, getpgrp());

  return 0;
}
