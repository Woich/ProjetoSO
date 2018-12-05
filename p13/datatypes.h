// PingPongOS
// Prof. Marco Aurelio, DAINF UTFPR
// 
// Marcelo Guimarães da Costa
// Pedro Henrique Woiciechovski

#ifndef __DATATYPES__
#define __DATATYPES__

#include <ucontext.h>

// Estrutura que define uma tarefa
typedef struct task_t {
	
	//estrutura da fila (tem que vir por primeiro)
	struct task_t* prev;
	struct task_t* next;
	
	int tid;
	
	//ponteiros importantes
	struct task_t** fila;
	struct task_t* main;
	
	struct task_t* filaJoin;
	
	//variavel de contexto 
	ucontext_t Contexto;

	//sistema de prioEstaticaridades
	int prioEstatica;
	int prioDinamica;

	//tempos
	unsigned int momentoCriacao;
	unsigned int momentoUltimaExecucao;
	
	unsigned int tempoExecucao;
	unsigned int tempoProcessando;
	unsigned int nmroAtivacoes;

	unsigned int tempoSoneca;
	
	//controle
	char estado;
	int codigoSaida;

} task_t ;

// estrutura que define um semáforo
typedef struct semaphore_t {
    struct task_t* fila;
    int valor;

    unsigned char isAtivo;
} semaphore_t ;

// estrutura que define um mutex
typedef struct mutex_t {
    struct task_t* fila;
    unsigned char valor;
    
    unsigned char isAtivo;
} mutex_t ;

// estrutura que define uma barreira
typedef struct barrier_t {
    struct task_t* fila;
    int tarefasMaximo;
    int tarefasContagem;
    
    unsigned char isAtivo;
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct mqueue_t {
    void* content;
    int mensagemTamanho;
    int mensagensTamanho;
    int mensagensContador;
    
    semaphore_t semaforoBuffer;
    semaphore_t semaforoItem;
    semaphore_t semaforoVaga;
    
    unsigned char isAtivo;
} mqueue_t ;

#endif
