#ifndef COMMAND_H_DEFINED_4377734146534469
#define COMMAND_H_DEFINED_4377734146534469

enum {
  SH_ARG_MAX = 64,      /// Maximum number of arguments a Command can hold
  SH_PIPELINE_MAX = 32  /// Maximum number of commands a Pipeline can hold
};

/**
* Stores information about a single command
**/
typedef struct Command {
  char* cmd;              /// Command name
  char* args[SH_ARG_MAX]; /// Arguments
  char* input;            /// Input redirection
  char* output;           /// Output redirection
  int   pid;              /// Process Id
} Command;

/**
* Stores information about a pipeline
**/
typedef struct Pipeline {
  Command* cmds[SH_PIPELINE_MAX]; /// List of commands in the pipeline
  char* cmdline;                  /// Command line used to start the process
  int bg;                         /// True if it should start in the background
  int gid;                        /// Pipeline GID
} Pipeline;

/**
* Creates a new, initialized Command
* \return A pointer to the newly created Command object
*/
Command* new_command();

/**
* Destroys a Command object
* \param cmd Pointer to a Command object
*/
void destroy_command(Command* cmd);

/**
* Gets the number of arguments in the given Command
* \param cmd Pointer to a Command object
* \return The number of arguments in the Command
*/
int cmd_argcount(Command* cmd);

/**
* Adds an argument to a Command object
* \param cmd The destination Command
* \param arg The argument to be added
* \return 1 if the call succeeds, 0 otherwise
*/
int cmd_addarg(Command* cmd, char* arg);

/**
* Creates a new, initialized Pipeline
* \return A pointer to the newly created Pipeline object
*/
Pipeline* new_pipeline();

/**
* Destroys a Pipeline object
* \param ppl Pointer to a Pipeline object
*/
void destroy_pipeline(Pipeline* ppl);

/**
* Gets the number of commands in the given Pipeline
* \param ppl Pointer to a Pipeline object
* \return The number of commands in the Pipeline
*/
int pipe_cmdcount(Pipeline* ppl);

/**
* Adds an command to a Pipeline object
* \param ppl The destination Pipeline
* \param cmd The command to be added
* \return 1 if the call succeeds, 0 otherwise
*/
int pipe_addcmd(Pipeline* ppl, Command* cmd);

#endif
