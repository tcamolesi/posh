#ifndef JOBCONTROL_H_DEFINED_7688473254764987
#define JOBCONTROL_H_DEFINED_7688473254764987

#define JT_NO_FG_JOB -1
#define SH_MAX_JOBS 32

#include "command.h"

enum JobState {
  SH_UNUSED = 0,
  SH_RUNNING,
  SH_STOPPED,
  SH_TERMINATED
};

typedef struct Job {
  Pipeline* ppl;
  int js;
} Job;

typedef struct JobTable {
  Job jobs[SH_MAX_JOBS];
  int first_free;
  int fg_job;
} JobTable;

int sh_foreground_job(int jid);
int sh_add_job(Pipeline *ppl);
int sh_find_job_by_pid(int pid);
void sh_update_jobs();
void sh_print_job(int jid);


extern JobTable sh_jobs;

#endif
