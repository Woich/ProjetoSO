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
task_t tarefaMain, *tarefaAtual, *despachante;
//lista de tarefas prontas para execução e suspensas.
task_t **tarefasProntas, **tarefasSuspensas;

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
    tarefaMain.prioridade = 0;

	//tid da main eh 0, iniciando a sequencia
	nextTaskId = 1;

	//Criação do dispatcher/despachante
	task_create(&despachante,(void*) dispatcher_body, NULL);
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

    //Adicionando uma prioridade a tarefa
    task->prioridade = 0;

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

    //Adiciona a tarefa na lista de tarefas prontas
    queue_append((queue_t **) &tarefasProntas, (queue_t *) task);

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

    if(tarefaAtual != &despachante){
        //retorna para o despachante
        task_switch(&despachante);
    }else{
        //retorna para a main
        task_switch(&tarefaMain);
    }
}

void dispatcher_body (){
    task_t *proxima;
    //pega o endereço da main
    task_t *enderecoMain;
    enderecoMain = &tarefaMain;

    //Enquanto existir tarefas a serem executadas
    while(queue_size((queue_t *) tarefasProntas) > 0){
        //Inicialização de próximoa com primeira tarefa da lista de prontas.
        proxima = tarefasProntas;
        //Se existe uma próxima tarefa
        if(proxima != NULL){
            //Remove a tarefa da pilha de prontas para evitar que a mesma tarefa sempre seja a unica acionada
            queue_remove((queue_t **) &tarefasProntas, (queue_t *) proxima);

            //Se a próxima não for a main
            if(proxima != enderecoMain){
                //Altera a tarefa em execução
                task_switch(proxima);
            }

        }
    }

    task_exit(0);
}

void task_yield (){
    if(tarefaAtual != despachante){
        queue_append((queue_t **) &tarefasProntas, (queue_t *) tarefaAtual);
    }

    task_switch(&despachante);

}

void task_suspend (task_t *task, task_t **queue){}

void task_resume (task_t *task){}

//Essa função deve retornar a tarefa de maior prioridade
//void escalonamento(){}
