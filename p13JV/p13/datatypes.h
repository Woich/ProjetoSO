// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DAINF UTFPR
// Versão 1.0 -- Março de 2015
//
// Estruturas de dados internas do sistema operacional

#ifndef __DATATYPES__
#define __DATATYPES__

#include <ucontext.h>

// Estrutura que define uma tarefa
typedef struct task_t task_t;

struct task_t
{
  struct task_t *prev, *next;
  int tid;
  int prio;
  int initprio;
  int quantum;
  int activations;
  int init_time;
  int cpu_time;
  int join_exitcode;
  int sleep_wake;
  int auxStatus;
  ucontext_t context;
  task_t *waiting;
};

// estrutura que define um semáforo
typedef struct
{
  struct task_t *sem_queue;
  int sem_value;
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  int max_tasks;
  int task_count;
  struct task_t *b_queue;
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
    int max;
    int size;
    int count;
    void *msgs;
    semaphore_t *sm_limit;
    semaphore_t *sm_inuse;
    semaphore_t *sm_msgcount;

} mqueue_t ;

#endif
