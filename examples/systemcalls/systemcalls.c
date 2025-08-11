#include "systemcalls.h"
#include <stdlib.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

	openlog(NULL, 0, LOG_USER);
/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	int ret;
	if (cmd == NULL) {
		syslog(LOG_ERR, "systemcalls.c::do_system cmd was NULL");
		return false;
	}
	ret = system(cmd);
	if ( ret != 0 ) {
		syslog(LOG_ERR, "systemcalls.c::do_system system() call returned: %d", ret);
		return false;
	}
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
	openlog(NULL, 0, LOG_USER);
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
*/
	fflush(stdout);
	int status;
	pid_t pid;
	pid = fork();
	if ( pid == -1 ) {
		syslog(LOG_ERR, "systemcalls.c::do_exec error creating fork");
		return false;
	}
	if ( pid == 0 ) {
		syslog(LOG_DEBUG, "systemcalls.c::do_exec child PID: %d", getpid());
		syslog(LOG_DEBUG, "systemcalls.c::do_exec execv( %s, [%s, %s, %s, %s] )", command[0], command[0], command[1], command[2], command[3]);
		int ret = execv(command[0], command);
		if ( ret == -1 ){
			syslog(LOG_DEBUG, "systemcalls.c::do_exec return code: %d", ret);
			exit(EXIT_FAILURE);
		}
	}
	else 
	{
		waitpid(pid, &status, 0);
		if(WEXITSTATUS(status) != 0)
			return false;
		return true;
	}

    va_end(args);
	return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for (i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
//    command[count] = command[count];
	int fd = open(outputfile,O_WRONLY|O_TRUNC|O_CREAT,0644);
	if ( fd < 0 ) {
		perror("open");
		abort();
	}
		
	if (command[0]==NULL || command[0][0] != '/')
		return false;

	pid_t pid = fork();
	int status;
	
	if ( pid == -1 )
		return false;
	if ( pid == 0 )
	{	
		if( dup2(fd, 1) < 0 ) {
			perror("dup2");
			abort();
		}
		close(fd);
		if ( execv(command[0], command) == -1 )
			exit(EXIT_FAILURE);
		perror("execvp"); 
		abort();
	}
	else
	{
		waitpid(pid, &status, 0);
		if( 0 != WEXITSTATUS(status) )
			return false;
		return true;
	}

/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    va_end(args);

    return true;
}
