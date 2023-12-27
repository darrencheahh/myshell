#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define MAX_ARGUMENTS 10
#define MAX_ARGUMENT_LENGTH 50

void parse_input(const char* command, char* args[]){
  int args_count = 0; //counter for no. of arguments
  int args_length = 0; //counter for length of current argument

  for(int i = 0; command[i] != '\0'; i++){
    //if theres a space, newline or tab, mark the end of the string
    if(command[i] == ' ' || command[i] == '\t' || command[i] == '\n'){
      if(args_length > 0) { //check the argument has no-zero length    
        args[args_count] = malloc(args_length + 1); //allocate memory for argument(s)

        // copy characters into the argument
        memcpy(args[args_count], command + i - args_length, args_length);
        args[args_count][args_length] = '\0'; //mark end of string
        
        args_count++; //increase argument count
        args_length = 0; //resets to zero for next argument

        if(args_count >= (MAX_ARGUMENTS - 1)){ //reached maximum arguments
          break;
        } 
      }
    } else {
          args_length++;
    }
  } 
  args[args_count] = 0; //mark end of argument list
}

void display_prompt(){ //prints >>> for shell commands
  printf(">>> ");
}


void cd_func(char* args[]){ //cd function
  if(strcmp(args[1], "") != 0){ //checking if there is an argument
    if(chdir(args[1]) != 0){ 
      fprintf(2, "No such file or directory %s\n", args[1]);
    }
  } else {
    fprintf(2, "cd: missing argument, try again \n");
  }
}

void pipes2_execute(char* args[], int pipe_index1, int pipe_index2) {
    int pipe_fd1[2];
    int pipe_fd2[2];

    pipe(pipe_fd1);
    pipe(pipe_fd2);

    int pid1 = fork();
    if (pid1 == 0) {
        // Child process for the first command
        close(pipe_fd1[0]); // Close read end of the first pipe
        close(1);           // Close standard output
        dup(pipe_fd1[1]);   // Duplicate write end of the first pipe
        close(pipe_fd1[1]);  // Close the duplicated file descriptor

        exec(args[0], args); // Execute the first command
        fprintf(2, "Error executing %s\n", args[0]);
        exit(1);
    } else if (pid1 > 0) {
        // Parent process for the first command
        close(pipe_fd1[1]); // Close write end of the first pipe
        close(0);           // Close standard input
        dup(pipe_fd1[0]);   // Duplicate read end of the first pipe
        close(pipe_fd1[0]);  // Close the duplicated file descriptor

        int pid2 = fork();

        if (pid2 == 0) {
            // Child process for the second command
            close(pipe_fd2[0]); // Close read end of the second pipe
            close(1);           // Close standard output
            dup(pipe_fd2[1]);   // Duplicate write end of the second pipe
            close(pipe_fd2[1]);  // Close the duplicated file descriptor

            // Split the command and its arguments
            char* second_command_args[MAX_ARGUMENTS];
            int j = 0;
            for (int i = pipe_index1 + 1; i < pipe_index2; i++) {
                second_command_args[j++] = args[i];
            }
            second_command_args[j] = 0;

            exec(second_command_args[0], second_command_args); // Execute the second command
            fprintf(2, "Error executing %s\n", second_command_args[0]);
            exit(1);

        } else if (pid2 > 0) {
            // Parent process for the second command
            close(pipe_fd2[1]); // Close write end of the second pipe
            close(0);           // Close standard input
            dup(pipe_fd2[0]);   // Duplicate read end of the second pipe
            close(pipe_fd2[0]);  // Close duplicated file descriptor

            exec(args[pipe_index2 + 1], args + pipe_index2 + 1); // Execute the third command
            fprintf(2, "Error executing %s\n", args[pipe_index2 + 1]);
            exit(1);
        } else {
            fprintf(2, "fork error\n");
            exit(1);
        }
    } else {
        fprintf(2, "fork error\n");
        exit(1);
    }
}

void pipes_execute(char* args[], int pipe_index){
  args[pipe_index] = 0; //terminate the first command

  int pipe_fd[2]; 
  pipe(pipe_fd);

  int pid = fork();
  if(pid == 0){
    //child process for second command
    close(pipe_fd[1]); //close write end of the pipe
    close(0);//close standard input
    dup(pipe_fd[0]); //duplicate read end of pipe
    close(pipe_fd[0]); //close duplicated file descriptor

    exec(args[pipe_index + 1], args + pipe_index + 1); //execute the second command
    fprintf(2, "Error executing %s\n", args[pipe_index + 1]);
    exit(1);
  } else if (pid > 0){
      //parent process for the first command
      close(pipe_fd[0]); //close read end of pipe
      close(1); //close standard output
      dup(pipe_fd[1]); //duplicate write end of pipe
      close(pipe_fd[1]); 

      exec(args[0], args); //execute the first command
      fprintf(2, "Error executing %s\n", args[0]); 
      exit(1); 
  } else {
      fprintf(2, "fork error\n"); 
      exit(1);
  }
}

void execute_cmd(char* args[]){ //execute command for echo, ls, mkdir
  char* input_file = 0;
  char* output_file = 0;
  int fd = 1, num_pipes = 0, pipe_indices[MAX_ARGUMENTS];
  
  int pid = fork();
  if(pid > 0) {
    wait(0);
  } else if(pid == 0) {
      for(int i = 0; args[i] != 0; i++){ //check for pipes
        if(strcmp(args[i], "|") == 0){
          pipe_indices[num_pipes++] = i;
        }
      }
      //check for output redirection
      for(int i = 0; args[i] != 0 && args[i + 1] != 0; i++) {
        if(strcmp(args[i], ">") == 0){ // loop to find > symbol
          output_file = args[i+1]; // checking if argument after symbol is NULL
          if(output_file != 0){
            fd = open(output_file, O_WRONLY | O_CREATE); // open file
            if(fd < 0){
              fprintf(2, "Error opening file %s\n", args[i+1]);
              exit(1); 
            }
            //terminate arguments to NULL
            args[i] = 0; 
            args[i+1] = 0;
          }
        }
      }
      if(fd != 1){
        close(1); //close standard output
        dup(fd); //duplicate file
        close(fd);
      }
      //check for input redirection
      for(int i = 0; args[i] != 0 && args[i + 1] != 0; i++) {
        if(strcmp(args[i], "<") == 0){ // loop to find > symbol
            input_file = args[i+1]; // checking if argument after symbol is NULL
            if(input_file != 0){
              fd = open(input_file, O_RDONLY); // open file
              if(fd < 0){
                fprintf(2, "Error opening file %s\n", args[i+1]);
                exit(1); 
              }
              //terminate arguments to NULL
              args[i] = 0; 
              args[i+1] = 0;
            }
          }
        }
        if(fd != 1) {
        close(0); //close standard output
        dup(fd); //duplicate file
        close(fd);
        }

        if(num_pipes == 1){
          pipes_execute(args, pipe_indices[0]);
          return;
        } else if (num_pipes == 2){
            pipes2_execute(args, pipe_indices[0], pipe_indices[1]);
            return;
        }

        exec(args[0], args);// execute arguments
        fprintf(2, "Error executing %s\n", args[0]);
        exit(1);
    } else {
        fprintf(2, "fork error\n");
    }
}

int main() {
  char command[MAX_ARGUMENT_LENGTH];
  char* args[MAX_ARGUMENTS];

  for(int i = 0; i < MAX_ARGUMENTS; i++) { //set all elements in args to NULL
    args[i] = '\0';
  }

  while(1){
    display_prompt();
    gets(command, sizeof(command)); //reading the input
    parse_input(command, args); //making the input into args arrays of string

    if(strcmp(*args, "") != 0){
      if(strcmp(args[0], "cd") == 0){
        cd_func(args); //call cd function
      } else if(strcmp(args[0], "exit") == 0){
          exit(0); //exit from shell
      } else {
            execute_cmd(args); //execute command with redirection handling 
        }
      }
    }
  return 0;
}
  