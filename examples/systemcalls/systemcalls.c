#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int status = system(cmd);
    if(-1 == status)
    {
        perror("system");
        return false;
    }

    return (0 == status);
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
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    pid_t pid = fork();
    if(-1 == pid)
    {
        perror("fork");
        return false;
    }
    if(0 == pid)
    {
        // In the child process: execute the command
        execv(command[0], command);
        
        // If execv returns, an error occured
        perror(execv);
        exit(1);
    }
    else
    {
        // In the parent process: wait for the child to finish
        int status;
        if(-1 == waitpid(pid, &status, 0))
        {
            perror("waitpid");
            return false;
        }
        if(!WIFEXITED(status) || 0 != WEXITSTATUS(status))
        {
            return false;
        }
    }

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
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    int fd = creat(outputfile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(-1 == fd)
    {
        perror("open");
        return false;
    }

    pid_t pid = fork();
    if(-1 == pid)
    {
        perror("fork");
        close(fd);
        return false;
    }
    if(0 == pid)
    {
        // In the child process: redirect standard output to the file
        if (-1 == dup2(fd, STDOUT_FILENO))
        {
            perror("dup2");
            exit(1);
        }
        close(fd);
        execv(command[0], command);
        // If execv returns, an error occured
        perror("execv");
        exit(1);
    }
    else
    {
        close(fd);
        int status;
        if(-1 == waitpid(pid, &status, 0))
        {
            perror("waitpid");
            return false;
        }
        if (!WIFEXITED(status) || 0 != WEXITSTATUS(status))
        {
            return false;
        }
    }

    return true;
}
