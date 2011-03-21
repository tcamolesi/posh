#ifndef COMMAND_H_DEFINED_1484576472447541
#define COMMAND_H_DEFINED_1484576472447541

#include "command.h"

#define SH_MAX_JOBS 32

int sh_init();
int sh_process_pipeline(Pipeline* ppl);

typedef int (sh_builtin_func)(Command*);
typedef struct BuiltinCmd {
  char* cmd;
  sh_builtin_func *fn;
} BuiltinCmd;

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

extern BuiltinCmd sh_builtins[];
extern JobTable sh_jobs;
#endif
