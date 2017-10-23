#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

/*
 * Usage: ./a.out "echo hola"
 */

char **
create_arguments(int argc, char * argv[]){
  char ** args = malloc((argc+2)*sizeof(char*));
  args[0] = "bash";
  args[1] = "-c";
  int i;
  for (i = 1; i < argc; i++){
    args[i+1] = argv[i];
  }
  args[i+1] = NULL;
  // for (int i = 0; i < argc; i++){
  //   printf("%s\n", args[i]);
  // }
  return args;
}

int
main(int argc, char * argv[]){

  char ** args = create_arguments(argc, argv);
  int pid = fork();
  if (pid == -1){
    perror("fork error");
  }else if(pid == 0){
    int value = execve("/bin/bash", args, NULL);
    perror("execve");
    if (value == -1){
      printf("Soy un error\n");
    }
  } else {
    int ret_value;
    alarm(10);
    waitpid(pid, &ret_value, 0);
    printf("RET VALUE=%d\n",ret_value);
  }

}

