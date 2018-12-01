// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DAINF UTFPR
// Versão 1.0 -- Março de 2015
//
// Estruturas de dados internas do sistema operacional

#ifndef __DATATYPES__
#define __DATATYPES__

#define STACKSIZE 32768

#include <ucontext.h>

// Estrutura que define uma tarefa
// Estrutura que define uma tarefa
typedef struct task_t{
	struct task_t *prev, *next ; // para usar com a biblioteca de filas (cast)
	int tid ; // ID da tarefa
  ucontext_t Contexto; //contexto da tarefa
  //prioridades da tarefa
  int prioEstatica;
  int prioDinamica;
  //tarefa de sistema?
  int isTarefaSistema;
  int quantum;
  //tempos
  int inicioTempoExecucao;
  int tempoProcessamento;
  int nmroAtivacoes;
  //Tarefas que depdendem do final desta tarefa
  struct task_t *depedente;
  //código de saída da tarefa
  int codigoSaida;
  //Tempo de acrodar da tarefa
  int tempoAcordar;
  // demais informações da tarefa
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  // preencher quando necessário
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando necessário
} mqueue_t ;

#endif
