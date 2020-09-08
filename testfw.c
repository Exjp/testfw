  #define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "testfw.h"
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/time.h>

#define NSIGNORT 10


/* ********** STRUCTURES ********** */

struct testfw_t
{
    char* program;
    int timeout;
    char* logfile;
    char* cmd;
    bool silent;
    bool verbose;
    struct test_t** tab_test;
    int nb_test;

};

/* ********** FRAMEWORK ********** */

struct testfw_t* testfw_init(char* program, int timeout, char* logfile, char* cmd, bool silent, bool verbose)
{
  //checking for illegal paramaters
  if(program == NULL)
  {
    fprintf(stderr,"testfw_init illegal parameter (program)\n");
    exit(EXIT_FAILURE);
  }
  if(timeout <= 0){
    fprintf(stderr,"testfw_init illegal parameter (timeout)\n");
    exit(EXIT_FAILURE);
  }

  struct testfw_t* p = malloc(sizeof(struct testfw_t));
  if(p == NULL)
  {
    fprintf(stderr, "testfw_init: memory allocation failed(fw)");
  	exit(EXIT_FAILURE);
  }

  p -> program = program;
  p -> timeout = timeout;
  p -> logfile = logfile;
  p -> cmd = cmd;
  p -> silent = silent;
  p -> verbose = verbose;

  p -> tab_test = malloc(sizeof(struct testfw_t*) * 30);
  if (p -> tab_test == NULL)
  {
  	fprintf(stderr, "testfw_init: memory allocation failed (g->tab_test)");
  	exit(EXIT_FAILURE);
  }

  p->nb_test = 0;

  return p;
}

void testfw_free(struct testfw_t* fw)
{
  if(fw != NULL)
  {
    if(fw -> tab_test != NULL)
    {
      for(int i = 0; i < fw -> nb_test; i++)
      {
        if(fw -> tab_test[i] != NULL)
        {
          if(fw -> tab_test[i] -> name != NULL)
          {
            free(fw -> tab_test[i] -> name);
          }
          if(fw -> tab_test[i] -> suite != NULL)
          {
            free(fw -> tab_test[i] -> suite);
          }
          free(fw -> tab_test[i]);
        }
      }
      free(fw -> tab_test);
    }
    free(fw);
  }
}

int testfw_length(struct testfw_t* fw)
{
  // checking for illegal paramaters
  if(fw == NULL)
  {
    fprintf(stderr, "testfw_length illegal paramater\n");
    exit(EXIT_FAILURE);
  }
  return fw -> nb_test;
}

struct test_t *testfw_get(struct testfw_t* fw, int k)
{
  // checking for illegal paramaters
  if(fw == NULL)
  {
    fprintf(stderr, "testfw_get illegal paramater (fw)\n");
    exit(EXIT_FAILURE);
  }
  if(k < 0){
    fprintf(stderr, "testfw_get illegal paramater (k)\n");
    exit(EXIT_FAILURE);
  }
  return fw -> tab_test[k];
}

/* ********** REGISTER TEST ********** */

struct test_t* testfw_register_func(struct testfw_t* fw, char* suite, char* name, testfw_func_t func)
{
  // checking for illegal paramaters
  if(fw == NULL){
    fprintf(stderr, "testfw_register_func illegal paramater (fw)\n");
    exit(EXIT_FAILURE);
  }
  if(suite == NULL){
    fprintf(stderr, "testfw_register_func illegal paramater (suite)\n");
    exit(EXIT_FAILURE);
  }
  if(name == NULL){
    fprintf(stderr, "testfw_register_func illegal paramater (name)\n");
    exit(EXIT_FAILURE);
  }

  struct test_t* test = malloc(sizeof(struct test_t));
  if(test == NULL)
  {
    fprintf(stderr, "testfw_register_func: memory allocation failed(test)");
  	exit(EXIT_FAILURE);
  }

  test -> suite = malloc(sizeof(char*));
  if (test -> suite == NULL)
  {
    fprintf(stderr, "testfw_register_func: memory allocation failed(test -> suite)");
    exit(EXIT_FAILURE);
  }

  test -> name = malloc(sizeof(char*));
  if (test -> name == NULL)
  {
    fprintf(stderr, "testfw_register_func: memory allocation failed(test -> name)");
    exit(EXIT_FAILURE);
  }

  strcpy(test -> name, name);
  strcpy(test -> suite, suite);
  test -> func = func;

  fw -> tab_test[fw -> nb_test] = test;
  fw -> nb_test++;

  return test;
}

struct test_t* testfw_register_symb(struct testfw_t* fw, char* suite, char* name)
{
  // checking for illegal paramaters
  if(fw == NULL)
  {
    fprintf(stderr, "testfw_register_symb illegal paramater (fw)\n");
    exit(EXIT_FAILURE);
  }
  if(suite == NULL)
  {
    fprintf(stderr, "testfw_register_symb illegal paramater (suite)\n");
    exit(EXIT_FAILURE);
  }
  if(name == NULL)
  {
    fprintf(stderr, "testfw_register_symb illegal paramater (name)\n");
    exit(EXIT_FAILURE);
  }

  void* handle;
  handle = dlopen(fw->program, RTLD_NOW);

  char symbol[256];
  snprintf(symbol, 256,"%s_%s", suite, name);
  testfw_func_t test_funct;
  //printf("1/test_funct       %p\n", test_funct);
  *(void **)(&test_funct) = dlsym(handle, symbol);
  //printf("2/test_funct       %p\n", test_funct);

  struct test_t* test = testfw_register_func(fw, suite, name, test_funct);

  return test;

}

int testfw_register_suite(struct testfw_t* fw, char* suite)
{
  // checking for illegal paramaters
  if(!fw || !suite)
  {
    fprintf(stderr, "testfw_register_suite illegal paramater (fw)\n");
    exit(EXIT_FAILURE);
  }

  int cpt = 0;
  char comd[256];
  char tab[256];
  char* exe = fw -> program;

  snprintf(comd,256,  "nm --defined-only %s | cut -d ' ' -f 3 | grep \"^%s_\"", exe, suite);
  FILE* fd = popen(comd, "r");

  while(fgets(tab,sizeof(tab), fd) != NULL)
  {
    char* tmp_suite = strtok(tab,"_");
    char* tmp_name = strtok(NULL,"\n");
    if(testfw_register_symb(fw,tmp_suite,tmp_name) != NULL)
    {
      cpt++;
    }
  }

  pclose(fd);
  return cpt;
}

/* ********** RUN TEST ********** */

pid_t pid_fils;
void F1(int sig){
  kill(pid_fils, SIGINT);
}



int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode)
{
  if(fw == NULL)
  {
    fprintf(stderr, "testfw_register_suite illegal paramater (fw)\n");
    exit(EXIT_FAILURE);
  }
  //usage ?


  int Save_1 = 0;
  int Save_2 = 0;
  int status;
  int cpt = 0;
  struct timeval start, end;
  struct sigaction sa, old;
  int fd_logfile;
  FILE* fd_cmd;


  if(fw -> logfile != NULL){
    fd_logfile = open(fw -> logfile, O_WRONLY | O_CREAT, 0644);
    if(fd_logfile != -1){
      Save_1 = dup(STDOUT_FILENO);
      Save_2 = dup(STDERR_FILENO);
      dup2(fd_logfile, STDOUT_FILENO);
      dup2(fd_logfile, STDERR_FILENO);
      close(fd_logfile);
    }
  }

  if(fw -> cmd != NULL){
    Save_1 = dup(STDOUT_FILENO);
    Save_2 = dup(STDERR_FILENO);
    fd_cmd = popen(fw-> cmd, "w");
    int fd_cmd_int = fileno(fd_cmd);
    dup2(fd_cmd_int, STDOUT_FILENO);
    dup2(fd_cmd_int, STDERR_FILENO);
    close(fd_cmd_int);
  }


  if(mode == TESTFW_FORKS){

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = F1;
    sigaction(SIGALRM, &sa, &old);
    for(int i = 0; i < fw -> nb_test; i++){
      gettimeofday(&start, NULL);
      pid_fils = fork();
      if(pid_fils == 0){
        if(fw -> silent == true){
          close(STDOUT_FILENO);
          close(STDERR_FILENO);
        }
        sigaction(SIGALRM, &old, NULL);
        if(fw->verbose == true){
          printf("%d: la fonction dans le fils %d\n",i, getpid());
        }
        exit(fw -> tab_test[i] -> func(argc, argv));
      }

      alarm(fw->timeout);
      wait(&status);

      gettimeofday(&end, NULL);

      float time = ((end.tv_sec - start.tv_sec)) * 1000.0 + ((end.tv_usec - start.tv_usec) / 1000.0);

      if(fw -> verbose == true){
        printf("\n");
        printf("                                t = %lf ms\n",time);
        printf("                                status = %d;\n                                WIFEXITED(status) = %d;\n                                WTERMSIG(status) =  %d;\n                                strsignal(status) = %s;\n                                strsignal( WTERMSIG(status)) = %s;\n",status, WIFEXITED(status), WTERMSIG(status), strsignal(status), strsignal( WTERMSIG(status)));
        printf("\n");
      }

      if(fw -> cmd != NULL){
        if(pclose(fd_cmd) == EXIT_SUCCESS){
          dprintf(STDOUT_FILENO, "[SUCCESS]");
        }
        else{
          dprintf(STDOUT_FILENO, "[FAILURE]");
        }
        pclose(fd_cmd);
      }
      else{
        if(WIFEXITED(status) == 1){
          if(WEXITSTATUS(status) == EXIT_SUCCESS){
            dprintf(STDOUT_FILENO, "[SUCCESS] run test \"%s.%s\" in %lf ms (status %d)\n",fw->tab_test[i] -> suite, fw -> tab_test[i] -> name, time, status);
          }
          else{
            dprintf(STDOUT_FILENO,"[FAILURE] run test \"%s.%s\" in %lf ms (status %d)]\n",fw->tab_test[i] -> suite, fw -> tab_test[i] -> name, time, WIFEXITED(status));
            cpt++;
          }
        }
        else{
          if(status == 2 && time >= fw -> timeout){
            dprintf(STDOUT_FILENO,"[TIMEOUT] run test \"%s.%s\" in %lf ms (status 124)\n",fw->tab_test[i] -> suite, fw -> tab_test[i] -> name, time);
          }
          else{
            dprintf(STDOUT_FILENO,"[KILLED] run test \"%s.%s\" in %lf ms (signal \"%s\")\n",fw->tab_test[i] -> suite, fw -> tab_test[i] -> name,time, strsignal( WTERMSIG(status)));
          }
          cpt++;
        }
      }
    }
    if(fw->logfile){
      dup2(Save_1, STDOUT_FILENO);
      dup2(Save_2, STDERR_FILENO);
    }
  }

  else if(mode == TESTFW_FORKP){
    //int nb_fork = fw -> nb_test;
    //for(int nb = 0; nb < nb_fork; nb++){

  }

  //printf("cpt = %d", cpt);
  return cpt;
}
