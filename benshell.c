#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#define MAXTOKS 500
#define MAXTOKSIZE 500

struct name {
  char** tok;
  int count;
  int status;
};

enum status_value { NORMAL, EOF_FOUND, INPUT_OVERFLOW, OVERSIZE_TOKEN };

/* Child process for pipe command */

int launchProcess(char *const *argv, int in_fd, int out_fd) {
  pid_t process;
  if ((process = fork()) == 0) {
      if (0!=in_fd) {
	dup2(in_fd, 0);
      }
      if (1!=out_fd) {
	dup2(out_fd, 1);
      }
      return execvpe(argv[0], argv, NULL);
  }
  else if (process < 0) {
    perror("execvpe");
  }
  return process; // return pid to parent
}

/* Launches a pipeline of processes for pipe command */

int launchPipeline(char **commands[]) {
  int i=0; int in_fd, pipe_fds[2];
  in_fd=0; // start from stdin
  /* Launches each command as a segment of the pipeline */
  while(commands[i] != NULL) {
    if(commands[1+i] == NULL) { // Last command
      return launchProcess(commands[i], in_fd, 1); // to stdout
    }
    pipe(pipe_fds);
    launchProcess(commands[i], in_fd, pipe_fds[1]);
    close(pipe_fds[1]);
    in_fd=pipe_fds[0];
    ++i;
  }
  return -1;
}

int read_name(struct name *n, char **envp) {

  int quit=1;

  /* Get input */

  while(quit == 1) {

    /* Initialize variables */

    int count=0;
    int count2=0;
    int buffer_counter=0;
    int i=0;
    char buffer[256];
    char* ret_val;

    n->tok = (char**) malloc(MAXTOKS); // allocation
    char ***commands = malloc((MAXTOKSIZE)*sizeof(char**)); // allocation for pipe command
    printf("Ben_shell$ ");
    ret_val=fgets(buffer, 256, stdin);

    /* Begin parsing */

    while(count<=MAXTOKS) {

      /* Allocate an array of char* to each variable in n->tok */

      n->tok[count] = (char*) malloc(MAXTOKSIZE);

      /* Check if it's a null byte or a new line */

      if(buffer[buffer_counter] == '\0') {
	n->count = count;
	break;
      }
      else if(buffer[buffer_counter] == '\n'){
	n->count = count;
	break;
      }

      /* Second for loop to count the # of spaces */

      else {
	for(count2=0; count2<MAXTOKSIZE; ++count2) {
	  if(isspace(buffer[buffer_counter])) // checks if its a space
	    ++buffer_counter; // counts # of spaces
	  else
	    break;
	}

	/* Loop through MAXTOKSIZE */
      
	for(i=0; i<=MAXTOKSIZE; ++i,++buffer_counter) {
	  if(isspace(buffer[buffer_counter])) { // checks if its a space
	    break;
	  }
	  else {
	    n->tok[count][i] = buffer[buffer_counter]; // puts this in n->tok if not
	  }
	}
	count++;
      }
    }
    /* Error checking */
    if(ret_val !=NULL) {
      n->status = 1;
    }
    else {
      n->status=0;
    }

    /* PIPE CODE */

    int x=0; // counter variable x
    for(x=0; x<=count; ++x) {

      /* Loops through all characters and checks for a pipe */

      if(n->tok[x][0]=='|') {

	/* Initialize some variables only needed if it's a pipe command */

	int pipe_start=0;
	int pipe_end=0;
	int pipe_count2=0;
	int it_cmds=0;

	while (pipe_count2<=count) {
	  if(n->tok[pipe_count2][0]=='|') {
	    pipe_end=pipe_count2-1;
	    int cmdTokCount = (pipe_end - pipe_start) + 1;
	    commands[it_cmds] = malloc((cmdTokCount+1)*sizeof(char*));
	    int it_toks=pipe_start;
	    int it_cmd=0;
	    while(it_toks<=pipe_end) {
	      commands[it_cmds][it_cmd]=malloc((strlen(n->tok[it_toks])+1)*sizeof(char));
	      strcpy(commands[it_cmds][it_cmd], n->tok[it_toks]);
	      ++it_toks;
	      ++it_cmd;
	    }
	    commands[it_cmds][cmdTokCount]=NULL;
	    pipe_start=i+1;
	    ++it_cmd;
	  }
	  ++pipe_count2;
	}
	pid_t final_process = launchPipeline(commands);
	if (final_process > 0) {
	  wait(NULL);
	}

	--it_cmds;
	while(it_cmds >= 0) {
	  int it_cmd=0;
	  while(commands[it_cmds][it_cmd] != NULL) {
	    free(commands[it_cmds][it_cmd]);
	    ++it_cmd;
	  }
	  free(commands[it_cmds]);
	  --it_cmds;
	}
	free(commands);
      }
    }

    /* System calls */ 

    int ret = fork();
    if (ret < 0) {
      printf("fork() FAILED");
      perror("forkeg");
      exit(1);
    }

    /* Fork succeeded */
    
    if (ret !=0) {
      wait(&ret);
    }
    else {

      /* chdir command */

      if(!strcmp("cd", n->tok[0])) {
	chdir(n->tok[1]);
      }

      /* time command */

      else if(!strcmp("time", n->tok[0])) {
	time_t timer;
	char* c_time_string;
	char bday[11];
	timer = time(NULL);
	c_time_string = ctime(&timer);
	(void) printf("Current time is: %s", c_time_string);
        strncpy(bday, c_time_string, 10);
	bday[10] = 0;
	if(!strcmp("Jun 24", bday+4)) {
	  printf("Today is Ben's birthday!\n");
	}
      }

      /* exit command */

      else if(!strcmp("exit", n->tok[0])) {
        printf("Hit ctrl+C to exit. Goodbye!\n");
	_exit(1);
      }

      /* no chdir exit or time command */

      else {
	/* automatically adds /usr/bin/ to the path */
	char temp_cmd[256];
	strcpy(temp_cmd, "/usr/bin/");
	strcat(temp_cmd, n->tok[0]);
	n->tok[0] = temp_cmd;
	n->tok[count] = 0;
	execve(n->tok[0], n->tok, envp);
	/* Technically doesn't work with anything not in /usr/bin/ */
	/* But it very easily could. */
	fprintf(stderr, "That system call doesn't exist. It must not be in /usr/bin/\n");
	exit(1);
      }
    }
  }
  return n->status;
}

int main(int argc, char **argv, char **envp) {
  struct name n;
  read_name(&n, envp);
  return 0;
}
