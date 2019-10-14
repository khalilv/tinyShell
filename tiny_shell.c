#define _XOPEN_SOURCE
#define _POSIX_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdbool.h>

//TO RESOLVE IMPLICIT REFERENCES
int length(char *line);
int my_system(char *line);
char *get_a_line();
int add_to_history(char *command);
int printHistory();

//DECLARE STATIC VARIABLES
static char *history[100];
static int history_number = 0;
static bool isFifo = false;
static char *namedFifo;

int my_system(char *line)
{

    add_to_history(line); //ADD LINE TO HISTORY ARRAY

    //CREATE TWO COPYS OF LINE
    char *copy = malloc(strlen(line) + 1);
    strcpy(copy, line);
    char *copy2 = malloc(strlen(line) + 1);
    strcpy(copy2, line);
    char *command;
    char *args;

    //SPLIT LINE UP BASED ON A SPACE. FIRST PART IS THE COMMAND, SECOND IS THE ARGUMENT
    command = strtok(line, " \0");
    args = strtok(NULL, "\0");
    int i = 0;

    if (args == NULL)
    {
        while (command[i] != 10)
        {
            i++;
        }
        command[i] = '\0';
    }
    else
    {
        while (args[i] != 10)
        {
            i++;
        }
        args[i] = '\0';
    }

    //IF USER TYPES CHDIR WITHOUT AN ARGUMENT
    if (args == NULL && strcmp(command, "chdir") == 0)
    {
        printf("No input arguments for chdir. Please specify the path you wish to navigate to.\n");
        return 0;
    }
    //IF USER TYPES CHDIR WITH AN ARGUMENT
    else if (strcmp(command, "chdir") == 0)
    {

        char path[BUFSIZ];
        int result = chdir(args);
        if (result == 0)
        {
            getcwd(path, sizeof(path));
            printf("Succesfully changed directory. You are now working in %s\n", path);
            return 0;
        }
        else
        {
            getcwd(path, sizeof(path));
            printf("Error changing directory. You are still working in %s\n", path);
            return 0;
        }
    }

    //IF USER TYPES HISTORY
    else if (strcmp(command, "history") == 0)
    {
        printHistory();
        return 0;
    }

    // IF USER TYPES LIMIT WITHOUT AN ARGUMENT
    else if (strcmp(command, "limit") == 0 && args == NULL)
    {
        printf("Please specify an argument\n");
        return 0;
    }
    //IF USER TYPES LIMIT WITH AN ARGUMENT. SET LIMIT OF RLIMIT_DATA ACCORDINGLY
    else if (strcmp(command, "limit") == 0)
    {
        struct rlimit oldlim;
        struct rlimit newlim;
        getrlimit(RLIMIT_DATA, &oldlim);
        int lim = atoi(args);
        newlim.rlim_cur = (unsigned long long)lim;
        newlim.rlim_max = oldlim.rlim_max;
        if (setrlimit(RLIMIT_DATA, &newlim) == 0)
        {
            printf("Hard limit value of RLIMIT_DATA is now %llu\n", (unsigned long long)newlim.rlim_max);
            printf("Soft limit value of RLIMIT_DATA is now %llu\n", (unsigned long long)newlim.rlim_cur);
        }
        else
        {
            printf("Error setting limit\n");
        }
        return 0;
    }
    //BONUS INTERNAL COMMAND TO CHECK LIMITS
    else if (strcmp(command, "checklim") == 0)
    {
        struct rlimit rll;
        getrlimit(RLIMIT_DATA, &rll);
        printf("Hard limit of RLIMIT_DATA is: %llu\n", (unsigned long long)rll.rlim_max);
        printf("Soft limit of RLIMIT_DATA is: %llu\n", (unsigned long long)rll.rlim_cur);
        return 0;
    }
    //IF YOU DO NOT ENTER AN INTERNAL COMMAND AND ARE RUNNING IN FIFO MODE.
    else if (!isFifo)
    {
        pid_t pid = fork();

        //FORK ERROR
        if (pid == -1)
        {
            printf("Error forking\n");
            return 0;
        }

        if (pid == 0)
        {
            //CHILD
            int a = execl("/bin/sh", "sh", "-c", copy, (char *)0);
            if (a == -1)
            {
                printf("Error executing command");
                return 0;
            }
        }
        else
        {
            //PARENT
            wait(NULL);
            printf("Child Done\n");
            return 0;
        }
    }
    else
    {
        //YOU ARE IN FIFO MODE
        //SPLIT LINE BASED ON | AND DELIMIT WITH \0
        char *fifoCommand = strtok(copy2, "|\0");
        char *fifoArgs = strtok(NULL, "\0");
        int i = 0;
        if (fifoArgs == NULL)
        {
            while (fifoCommand[i] != 10)
            {
                i++;
            }
            command[i] = '\0';
        }
        else
        {
            while (fifoArgs[i] != 10)
            {
                i++;
            }
            fifoArgs[i] = '\0';
        }

        //ERROR CHECKING. IF YOU ARE IN FIFO MODE I HAVE DESIGNED SUCH THAT YOU CAN ONLY ENTER INTERNAL COMMANDS AND PIPED COMMANDS.
        if (fifoArgs == NULL)
        {
            printf("FIFO MODE ONLY SUPPORTS INTERNAL COMMANDS AND PIPED COMMANDS. No second command specified. Please format as command | command or enter an internal command\n");
            return 0;
        }
        if (fifoCommand == NULL)
        {
            printf("FIFO MODE ONLY SUPPORTS INTERNAL COMMANDS AND PIPED COMMANDS. No command specified. Please format as command | command or enter an internal command\n");
            return 0;
        }

        pid_t pid = fork();
        //FORK ERROR
        if (pid == -1)
        {
            printf("Error forking\n");
            return 0;
        }
        if (pid == 0)
        {
            //CHILD
            close(1);
            close(2); 
            int fd = open(namedFifo, O_WRONLY); //OPEN FIFO FOR WRITING
            char in[BUFSIZ];
            fflush(stdout);
            dup(STDOUT_FILENO);
            dup(STDERR_FILENO); 
            freopen(namedFifo, "w", stdout);
            setvbuf(stdout, in, _IOFBF, BUFSIZ);
            int a = execl("/bin/sh", "sh", "-c", fifoCommand, (char *)0); //EXECUTE COMMAND. RESULT WILL GO IN FIFO
            close(fd);
        }
        else
        {
            //PARENT
            pid_t pid2 = fork();
            //ERROR FORKING
            if (pid2 == -1)
            {
                printf("Error forking\n");
                return 0;
            }
            if (pid2 == 0)
            {
                //SECOND CHILD
                close(0);
                int fd = open(namedFifo, O_RDONLY); //OPEN FIFO FOR READING
                char out[BUFSIZ];
                fflush(stdout);
                fflush(stdin);
                dup(STDIN_FILENO);
                freopen(namedFifo, "r", stdin);
                setvbuf(stdin, out, _IOFBF, BUFSIZ);
                int a = execl("/bin/sh", "sh", "-c", fifoArgs, (char *)0); //EXECUTE COMMAND TAKING AS INPUT THE FIFO CONTENTS
                close(fd);
            }
            else
            {
                //PARENT
                wait(NULL);
                printf("Child Done in Fifo mode\n");
            }
        }
    }
    free(copy);
    free(copy2);
    return 0;
}

//METHOD TO COMPUTE THE LENGTH OF A LINE
int length(char *line)
{
    if (line == NULL)
    {
        return 0;
    }
    int len = 0;
    char *ptr;
    ptr = line;
    while (*ptr != '\0')
    {
        len++;
        ptr++;
    }
    return len;
}

//METHOD TO GET A LINE FROM USER
char *get_a_line()
{
    static char buf[BUFSIZ];
    if (fgets(buf, sizeof(buf), stdin) == NULL)
    {
        //IF FGETS FAILS YOU ARE AT THE END OF THE FILE
        printf("End of File. Terminating\n");
        exit(0);
    }
    else
    {
        return buf;
    }
}

//ADD COMMAND TO HISTORY ARRAY
int add_to_history(char *command)
{
    if (history_number < 100)
    {
        history[history_number++] = strdup(command);
    }
    else
    //HISTORY ARRAY IS FULL SO DELETE THE FIRST ONE, SHIFT ALL OF THE ELEMENTS DOWN ONE AND ADD COMMAND TO END
    {
        free(history[0]);
        for (int i = 1; i < 100; i++)
        {
            history[i - 1] = history[i];
        }
        history[100 - 1] = strdup(command);
    }
    return 0;
}

//DISPLAY THIS HISTORY ARRAY
int printHistory()
{
    int i = 0;
    printf("\nHistory of commands from oldest to latest:\n");
    while (history[i] != NULL && i != 100)
    {
        printf("%s", history[i]);
        i++;
    }
    printf("\n");
    return 0;
}

//METHOD TO HANDLE SIGNALS THAT ARISE FROM CTRL C AND CTRL Z
static void sig_handler(int sig, siginfo_t *siginfo, void *context)
{
    if (sig == SIGINT)
    {
        printf("Would you like to terminate? Y/N \n");
        static char buf[BUFSIZ];
        read(STDIN_FILENO, buf, sizeof(buf));

        int i = 0;
        while (buf[i] != 10)
        {
            i++;
        }
        buf[i] = '\0';

        if (strcmp(buf, "Y") == 0 || strcmp(buf, "y") == 0)
        {
            printf("Terminating\n");
            exit(0);
        }
        else if (strcmp(buf, "N") == 0 || strcmp(buf, "n") == 0)
        {
            printf("Ignoring Signal. Please enter command: \n");
        }
        else
        {
            printf("Invalid Input. Ignoring Signal. Please enter command: \n");
        }
    }
    else if (sig == SIGTSTP)
    {
        printf("Ignoring Signal. Please enter command: \n");
    }
    return;
}

int main(int argc, char *argv[])
{
    //IF YOU SPECIFY AN ARGUMENT YOU GO INTO FIFO MODE.
    if (argc > 1)
    {
        isFifo = true;
        namedFifo = realpath(argv[1], namedFifo);
    }
    //SIGACTION WILL CALL SIG_HANDLER IF A SIGNAL ARISES
    struct sigaction customHandler;
    memset(&customHandler, '\0', sizeof(customHandler));
    customHandler.sa_sigaction = &sig_handler;
    customHandler.sa_flags = SA_RESTART;
    sigaction(SIGINT, &customHandler, NULL);
    sigaction(SIGTSTP, &customHandler, NULL);

    while (1)
    {
        char *line = get_a_line();
        if (length(line) > 1)
        {
            my_system(line);
        }
    }
}
