#include <stdio.h>
#include <stdlib.h>

#include <ucontext.h>

#include "pingpong.h"
#include "datatypes.h"
#include "queue.h"

// operating system check
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif


//macro global task main (encapsular a tarefa da main)
task_t tarefaMain, *tarefaAtual;

int nextTaskId;

//funçao de inicialização
void pingpong_init (){
	//tira o acesso ao buffer da tela (?)
	setvbuf (stdout, 0, _IONBF, 0) ;

	//tarefa atual aponta inicialmente para main, para apenas depois ir trocando
	tarefaAtual = &tarefaMain;

	//necessario inicializar a tarefa main
	tarefaMain.prev = NULL;
    tarefaMain.next = NULL;
    tarefaMain.tid = 0;

	//tid da main eh 0, iniciando a sequencia
	nextTaskId = 1;
}

//cria tarefas
int task_create(task_t *task, void (*start_func)(void *), void *arg){
	//salva o contexto atual na tarefa de retorno
	getcontext(&(task->Contexto));

	//aloca espaço para a pilha do contexto da tarefa
	char *stack;
    stack = malloc (STACKSIZE) ;

	//se stack não foi allocada corretamente
	if(!stack){
		 perror ("Erro na criação da pilha: ");
	    return(-1);
	}

	//dando um id e mudando o proximo id global
	task->tid = nextTaskId;
	nextTaskId++;

	//informações do contexto
	task->Contexto.uc_stack.ss_sp = stack ;
	task->Contexto.uc_stack.ss_size = STACKSIZE;
	task->Contexto.uc_stack.ss_flags = 0;
	task->Contexto.uc_link = 0;
	makecontext (&(task->Contexto), (void*)(*start_func), 1, arg);

	//informações de DEBUG
	#ifdef DEBUG
	    printf("task_create: criou tarefa %d\n", task->tid);
    #endif

	//a função deve retornar o id da tarefa ou um codigo
	//negativo no caso de erro
    return(task->tid);
}

//dá acesso ao processador para a tarefa indicada
int task_switch(task_t *task){
	if(task == NULL){
		perror ("Erro: tarefa não definida");
    	return(-1);
	}

	//troca a tarefa atualmente executada
	task_t *temp = tarefaAtual;
	tarefaAtual = task;

	//troca efetivamente o contexto
	swapcontext(&(temp->Contexto), &(tarefaAtual->Contexto));

	#ifdef DEBUG
		printf("task_switch: mudou a tarefa %d para a tarefa %d\n", temp->tid, tarefaAtual->tid);
	#endif
	return(0);
}

// retorna o identificador da tarefa atual 0 = main
int task_id(){
    #ifdef DEBUG
        printf("task_id: tarefa corrente eh a %d\n", tarefaAtual->tid);
    #endif
    return(tarefaAtual->tid);
}

// encerra a execução da tarefa ao retornar para a main. Exit code?
void task_exit(int exitCode){
    #ifdef DEBUG
	    printf("task_exit: exit da tarefa %d\n", tarefaAtual->tid);
    #endif

	//retorna para a main
    task_switch(&tarefaMain);
}
