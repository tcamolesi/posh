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

/**
* Stores information about a single job
**/
typedef struct Job {
  Pipeline* ppl;    ///Pointer to a Pipeline object associated with the job
  int js;           ///The state of the job
} Job;

/**
* Stores information about all active jobs
**/
typedef struct JobTable {
  Job jobs[SH_MAX_JOBS];  ///Array of jobs
  int first_free;         ///Index of the first free job slot in the array
  int fg_job;             ///Index of the current foreground job
} JobTable;

extern JobTable sh_jobs;

/**
* Brings a job to the foreground
*
* @param jid The id of the job to be foregrounded
* @return 0 if successful, -1 if an error occurs
*/
int sh_foreground_job(int jid);

/**
* Creates a new job and adds it to the job table
*
* @param ppl Pipeline associated with the job
* @return The id of the new job if successful, -1 if an error occurs
*/
int sh_add_job(Pipeline *ppl);

/**
* Finds the job associated with the given group id
*
* @param pid The group id to be searched
* @return The id of the job if successful, -1 if an error occurs
*/
int sh_find_job_by_pid(int pid);

/**
* Removes terminatted jobs from the job table and prints recently
* finished background jobs
*/
void sh_update_jobs();

/**
* Prints a formatted description of a job, using the format
* [JID] STATUS CMD_LINE
*
* @param jid The job to be printed
*/
void sh_print_job(int jid);

void sh_chld_handler(int sig, siginfo_t *siginfo, void *context);

#endif
