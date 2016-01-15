/*
 * tsh - A tiny shell program with job control
 *
 * The shell can start by ./tsh 
 * This shell can be used to call the excution files and run them either
 * in background or frontground. The shell supports several builtin 
 * commands including quit, jobs(list jobs and status), bg(restart 
 * the job and run in BG), fg(restart the job and run in FG), and the I/O
 * redirection. The shell has its own signal handlers to process three kinds
 * of signals SIGINT, SIGTSTP and SIGCHLD.
 *
 * Name: Haoyang Yuan
 * Andrew ID: haoyangy
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */


/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG} builtins;
};

/* End global variables */

/* Wrappers from csapp.c */
/* Process control wrappers */
pid_t Fork(void);
void Execve(const char *filename, char *const argv[], char *const envp[]);
pid_t Waitpid(pid_t pid, int *iptr, int options);
void Kill(pid_t pid, int signum);
void Setpgid(pid_t pid, pid_t pgid);

/* Signal wrappers */
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
void Sigdelset(sigset_t *set, int signum);
int Sigismember(const sigset_t *set, int signum);
int Sigsuspend(const sigset_t *set);
ssize_t Sio_puts(char s[]);
ssize_t Sio_putl(long v);
void Sio_error(char s[]);
ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
void sio_error(char s[]);


/* Unix I/O wrappers */
int Open(const char *pathname, int flags, mode_t mode);
void Close(int fd);
int Dup2(int fd1, int fd2);

/* Function prototypes */
void eval(char *cmdline);
/* Helper functions */
void fgbg_command(char **argv);
void waitfg(pid_t pid,sigset_t  prev_one);
/* Implemented handlers */
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list);
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid);
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);



/*
 * main - The shell's main routine
 */
int
main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */
    
    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    Dup2(1, 2);
    
    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }
    
    /* Install the signal handlers */
    
    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);
    
    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);
    
    /* Initialize the job list */
    initjobs(job_list);
    
    
    /* Execute the shell's read/eval loop */
    while (1) {
        
        
        if (emit_prompt) {
            fflush(stdout);
            Sio_puts(prompt);
            fflush(stdout);
        }
        
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) {
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        fflush(stdout);
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    }
    
    exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
void
eval(char *cmdline)
{
    int bg;
    struct cmdline_tokens tok;
    sigset_t  mask_one, prev_one, mask_all;
    int status;
    pid_t child_pid;
    
    /* Parse command line */
    bg = parseline(cmdline, &tok);
    
    if (bg == -1) /* parsing error */
        return;
    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;
    
    /* Process builtin functions */
    if(tok.builtins!=BUILTIN_NONE){
        
        if(tok.builtins==BUILTIN_QUIT){
            exit(0);
        }
        else if(tok.builtins==BUILTIN_JOBS){
            if(tok.outfile==NULL){
                listjobs(job_list,  1);
            }
            else{
                int fd = Open(tok.outfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                listjobs(job_list,  fd);
                Close(fd);
            }
        }
        else if(tok.builtins==BUILTIN_BG||tok.builtins== BUILTIN_FG){
            fgbg_command(tok.argv);
        }
        else{
            char* builtine = "no such builtin function";
            unix_error(builtine);
        }
        return;
    }
    
    /* Process file excution */
    /* Set signal block */
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_one); /* Block SIGCHLD */
    
    /* The child process */
    if ((child_pid = Fork()) == 0)
    {
        Sigprocmask(SIG_SETMASK, &prev_one, NULL); /* Unblock SIGCHLD */
        Setpgid(0, 0);
        
        /* I/O redirection */
        if(tok.outfile!=NULL){
            int fd = Open(tok.outfile,O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            Dup2(fd, 1);   // make stdout go to file
            Dup2(fd, 2);   // make stderr go to file
            Close(fd);
        }
        if(tok.infile!=NULL){
            int fd = Open(tok.infile,O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            Dup2(fd, 0);   // make stdout go to file
            Close(fd);
        }
        Execve(tok.argv[0], tok.argv, environ);
        exit(0);
    }
    
    
    /* Parent process */
    else{
        /* Add to job list */
        if(bg==1) status = BG;
        else status = FG;
        
        if( addjob(job_list,  child_pid, status, cmdline)==0 ){
            unix_error("addjob error");
            return;
        }
       
        int jid;
        if((jid = pid2jid(child_pid))==0){
            unix_error("pid2jid error");
            return;
        }
        
         /* Unblock other signals, except SIGCHLD for waitpid */
        /* wait for fg child process */
        if (!bg) {
            waitfg(child_pid,prev_one);
        }
        else printf("[%d] (%d) %s\n", jid,child_pid, cmdline);
    }
    
    /* Unblock SIGCHLD */
    fflush(stdout);
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);
    return;
}

/*
 * waitfg - wait for the fg process to terminate
 *
 * Parameters:
 *   pid:  The pid of the fg process
 *   pre_one: the original mask for signal, which doesn't stop SIGCHLD
 */
void waitfg(pid_t pid,sigset_t prev_one)
{
    if(verbose){
        Sio_puts("waitfg: Process (");
        Sio_putl(pid);
        Sio_puts(") no longer the fg process\n");
    }
    
    /* If the fg processes haven't been deleted */
    /* Use Sigsuspend to periodically unblock and wait for signal */
    while (pid==fgpid(job_list)){
        Sigsuspend(&prev_one);
        fflush(stdout);
    }
    return;
}

/*
 * fgbg - Execute the builtin_command about fg and bg
 *
 * Parameters:
 *  argv: The argv input from the terminal
 */
void fgbg_command(char** argv){
    
    /* Parse the parameters of fg and bg command */
    if(argv[1]==NULL){
        printf("%s command requires PID or %%jobid argument\n",argv[0]);
        return;
    }
    int jid=0;
    pid_t pid;
    char* argv1;
    
    /* check if it is jid */
    if(*argv[1]=='%'){
        argv1 =  argv[1]+1;
        if( (jid=atoi(argv1))==0){
            printf("%s: argument should be a PID or %%jobid\n",argv[0]);
            return;
        }
        struct job_t *job = getjobjid(job_list, jid);
        if(job->pid==0) {
            printf("%s: No such jobid\n",argv[1]);
            return;
        }
    }
    
    /* check if it is pid */
    else{
        argv1 = argv[1]+1;
        if((pid=atoi(argv1))==0){
            printf("%s: argument should be a PID or %%jobid\n",argv[0]);
            return;
        }
        int jid=pid2jid(pid);
        if(jid==0){
            printf("(%d): No such PID\n",pid);
            return;
        }
    }
    
    
    /* Mask the signal when process the joblist and send signals */
    sigset_t  mask_all, prev_one;
    Sigfillset(&mask_all);
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_one); /* Block SIGCHLD */
    
    struct job_t *job = getjobjid(job_list, jid);
    /* bg command, send signal to restart and set status BG */
    if(strcmp(argv[0],"bg")==0){
        printf("[%d] (%d) %s\n",jid,job->pid,job->cmdline);
        fflush(stdout);
        Kill(-job->pid,SIGCONT);
        job->state=BG;
    }
    
    /* fg command, send signal to restart and set status BG */
    else if(strcmp(argv[0],"fg")==0){
        Kill(-job->pid,SIGCONT);
        job->state=FG;
        waitfg(job->pid, prev_one);
    }
    
    Sigprocmask(SIG_SETMASK,&prev_one,NULL);
    return;
}
/*
 * parseline - Parse the command line and build the argv array.
 *
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters
 *             enclosed in single or double quotes are treated as a single
 *             argument.
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job
 *  -1:        if cmdline is incorrectly formatted
 *
 * Note:       The string elements of tok (e.g., argv[], infile, outfile)
 *             are statically allocated inside parseline() and will be
 *             overwritten the next time this function is invoked.
 */
int
parseline(const char *cmdline, struct cmdline_tokens *tok)
{
    
    static char array[MAXLINE];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */
    
    int parsing_state;                   /* indicates if the next token is the
                                          input or output file */
    
    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }
    
    (void) strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);
    
    tok->infile = NULL;
    tok->outfile = NULL;
    
    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;
    
    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;
        
        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }
        
        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
             quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }
        
        /* Terminate the token */
        *next = '\0';
        
        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
            case ST_NORMAL:
                tok->argv[tok->argc++] = buf;
                break;
            case ST_INFILE:
                tok->infile = buf;
                break;
            case ST_OUTFILE:
                tok->outfile = buf;
                break;
            default:
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
        }
        parsing_state = ST_NORMAL;
        
        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;
        
        buf = next + 1;
    }
    
    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }
    
    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;
    
    if (tok->argc == 0)  /* ignore blank line */
        return 1;
    
    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else {
        tok->builtins = BUILTIN_NONE;
    }
    
    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;
    
    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The
 *     handler reaps all available zombie children, but doesn't wait
 *     for any other currently running children to terminate.
 */
void
sigchld_handler(int sig)
{
    pid_t pid;
    int status;
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    
    if(verbose){
        Sio_puts("sigchld_handler: entering\n");
    }
    
    /* Reap all the terminated children */
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0){
        int jid = pid2jid(pid);
        
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        /* If child terminates by SIGINT */
        if (WIFSIGNALED(status)){
            jid=pid2jid(pid);
            if(jid!=0){
                deletejob(job_list,pid); /* Delete the child from the job list */
                if(verbose){
                    Sio_puts("sigchld_handler: Job [");
                    Sio_putl(jid);
                    Sio_puts("] (");
                    Sio_putl(pid);
                    Sio_puts(") deleted\n");
                }
                Sio_puts("Job [");
                Sio_putl(jid);
                Sio_puts("] (");
                Sio_putl(pid);
                Sio_puts(") terminated by signal 2\n");
                fflush(stdout);
            }
        }
        
        /* If child terminates by SIGTSTP */
        else if(WIFSTOPPED(status)){
            int jid = pid2jid(pid);
            struct job_t *job = getjobpid(job_list, pid);
            job->state = ST;
            Sio_puts("Job [");
            Sio_putl(jid);
            Sio_puts("] (");
            Sio_putl(pid);
            Sio_puts(") stopped by signal 20\n");
            fflush(stdout);
        }
        
        /* If child terminates normally */
        else {
            deletejob(job_list,pid); /* Delete the child from the job list */
            if(verbose){
                    Sio_puts("sigchld_handler: Job [");
                    Sio_putl(jid);
                    Sio_puts("] (");
                    Sio_putl(pid);
                    Sio_puts(") deleted\n");
                    Sio_puts("sigchld_handler: Job [");
                    Sio_putl(jid);
                    Sio_puts("] (");
                    Sio_putl(pid);
                    Sio_puts(") terminates OK (status ");
                    Sio_putl(status);
                    Sio_puts(")\n");
            }
        }
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    fflush(stdout);
    if(verbose){
        Sio_puts("sigchld_handler: exiting\n");
    }
    errno = olderrno;
    return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void
sigint_handler(int sig)
{
    pid_t pid;
    int olderrno = errno;
    
    if(verbose){
        Sio_puts("sigint_handler: entering\n");
    }
    
    /* Send SIGINT to child process */
    pid =fgpid(job_list);
    if(pid>0){
        Kill(-pid,SIGKILL);
        if(verbose){
            Sio_puts("sigint_handler: Job (");
            Sio_putl(pid);
            Sio_puts(") killed\n");
        }
    }
    
    if(verbose){
        Sio_puts("sigint_handler: exiting\n");
    }

    errno = olderrno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void
sigtstp_handler(int sig)
{
    
    int olderrno = errno;
    
    pid_t pid;
    if(verbose){
        Sio_puts("sigtstp_handler: entering\n");
    }
    
    /* Send SIGTSTP to child process and Set ST status*/
    pid =fgpid(job_list);
    if(pid>0){
        Kill(-pid,SIGTSTP);
        if(verbose){
            Sio_puts("sigtstp_handler: Job (");
            Sio_putl(pid);
            Sio_puts(") stopped\n");
        }
    }
    
    if(verbose){
        Sio_puts("sigtstp_handler: exiting\n");
    }
    
    errno = olderrno;
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void
initjobs(struct job_t *job_list) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int
maxjid(struct job_t *job_list)
{
    int i, max=0;
    
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline)
{
    int i;
    
    if (pid < 1)
        return 0;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                Sio_puts("Added job [");
                Sio_putl(job_list[i].jid);
                Sio_puts("] ");
                Sio_putl(job_list[i].pid);
                Sio_puts(" ");
                Sio_puts(job_list[i].cmdline);
                Sio_puts("\n");
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int
deletejob(struct job_t *job_list, pid_t pid)
{
    int i;
    
    if (pid < 1)
        return 0;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t
fgpid(struct job_t *job_list) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t
*getjobpid(struct job_t *job_list, pid_t pid) {
    int i;
    
    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid)
{
    int i;
    
    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int
pid2jid(pid_t pid)
{
    int i;
    
    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void
listjobs(struct job_t *job_list, int output_fd)
{
    int i;
    char buf[MAXLINE];
    
    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            switch (job_list[i].state) {
                case BG:
                    sprintf(buf, "Running    ");
                    break;
                case FG:
                    sprintf(buf, "Foreground ");
                    break;
                case ST:
                    sprintf(buf, "Stopped    ");
                    break;
                default:
                    sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                            i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void
usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void
app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;
    
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */
    
    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void 
sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}


/*************************************************
 * Wrappers for Process Control from csapp.c
 *************************************************/
pid_t Fork(void)
{
    pid_t pid;
    
    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}
/* $end forkwrapper */


pid_t Waitpid(pid_t pid, int *iptr, int options)
{
    pid_t retpid;
    
    if ((retpid  = waitpid(pid, iptr, options)) < 0)
        unix_error("Waitpid error");
    return(retpid);
}

void Setpgid(pid_t pid, pid_t pgid) {
    int rc;
    
    if ((rc = setpgid(pid, pgid)) < 0)
        unix_error("Setpgid error");
    return;
}

/* $begin kill */
void Kill(pid_t pid, int signum)
{
    int rc;
    
    if ((rc = kill(pid, signum)) < 0)
        unix_error("Kill error");
}
/* $end kill */


void Execve(const char *filename, char *const argv[], char *const envp[])
{
    if (execve(filename, argv, envp) < 0)
        unix_error("Execve error");
}

/*************************************************
 * Wrappers for Signal from csapp.c
 *************************************************/
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
        unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
        unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0)
        unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
        unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
        unix_error("Sigdelset error");
    return;
}

int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
        unix_error("Sigismember error");
    return rc;
}

int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/*************************************************
 * Wrappers for Sio from csapp.c
 *************************************************/
ssize_t Sio_putl(long v)
{
    ssize_t n;
    
    if ((n = sio_putl(v)) < 0)
        sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
    
    if ((n = sio_puts(s)) < 0)
        sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}
/* Private sio functions */

/* $begin sioprivate */
/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;
    
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}
static void sio_ltoa(long v, char s[], int b)
{
    int c, i = 0;
    
    do {
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);
    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;
    
    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */  //line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);                                      //line:csapp:sioexit
}


/*************************************************
 * Wrappers for Unix I/O routines fron csapp.c
 *************************************************/

int Open(const char *pathname, int flags, mode_t mode)
{
    int rc;
    
    if ((rc = open(pathname, flags, mode))  < 0)
        unix_error("Open error");
    return rc;
}


void Close(int fd)
{
    int rc;
    
    if ((rc = close(fd)) < 0)
        unix_error("Close error");
}


int Dup2(int fd1, int fd2)
{
    int rc;
    
    if ((rc = dup2(fd1, fd2)) < 0)
        unix_error("Dup2 error");
    return rc;
}
