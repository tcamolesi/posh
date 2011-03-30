#ifndef SHELLCONTROL_H_DEFINED_4377446344876871
#define SHELLCONTROL_H_DEFINED_4377446344876871

#include "command.h"
#include <signal.h>

/**
* Initializes signal handlers and internal shell variables
*
* @return 1 if successful, 0 otherwise
*/
int sh_init();

/**
* Executes all commands in a pipeline
*
* @param ppl Pipeline to be executed
* @return 1 if successful, 0 if the exit was executed, -1 in case of error
*/
int sh_process_pipeline(Pipeline* ppl);

#endif
