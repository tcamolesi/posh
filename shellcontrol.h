#ifndef SHELLCONTROL_H_DEFINED_4377446344876871
#define SHELLCONTROL_H_DEFINED_4377446344876871

#include "command.h"
#include <signal.h>

int sh_init();
int sh_process_pipeline(Pipeline* ppl);

void sh_chld_handler(int sig, siginfo_t *siginfo, void *context);
void sh_tstp_handler(int sig, siginfo_t *siginfo, void *context);

#endif
