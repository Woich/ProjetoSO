#include <stdio.h>
#include <stdlib.h>

#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>

#include "pingpong.h"
#include "datatypes.h"
#include "queue.h"

// operating system check
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#define ALFA -1
#define QUANTUM 20

//macro global task main (encapsular a tarefa da main)
task_t tarefaMain, tarefaDespachante, *tarefaAtual ;
//lista de tarefas prontas para execução e suspensas.
task_t *tarefasProntas, *tarefasSuspensas;

//id da proxima tarefa a ser criada
int nextTaskId;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;

//variaveis para controlar o tempo
int tempoTotal, tempoInicioTarefa, tempoFimTarefa;

// tratador do tempo
void tratador (int signum){
    tarefaAtual->tempoProcessamento++;
    tempoTotal++;

    if(tarefaAtual->tid != tarefaDespachante.tid){
        if(tarefaAtual->quantum >= 0){
            tarefaAtual->quantum--;
	    }else{
            task_yield();
	    }
    }
}

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
    tarefaMain.prioEstatica = 0;
    tarefaMain.prioDinamica = tarefaMain.prioEstatica;
    tarefaMain.isTarefaSistema = 0;

	//tid da main eh 0, iniciando a sequencia
	nextTaskId = 1;

	//Criação do dispatcher/despachante
	task_create(&tarefaDespachante,(void*) dispatcher_body, NULL);
    tarefaDespachante.isTarefaSistema = 1;


    // registra a para o sinal de timer SIGALRM
    action.sa_handler = tratador ;
    sigemptyset (&action.sa_mask) ;
    action.sa_flags = 0 ;
    if (sigaction (SIGALRM, &action, 0) < 0){
	    perror ("Erro em sigaction: ") ;
	    exit (1) ;
    }

    // ajusta valores do temporizador
    timer.it_value.tv_usec = 1000 ;      // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0 ;      // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000 ;   // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0 ;   // disparos subsequentes, em segundos

    // arma o temporizador ITIMER_REAL
    if (setitimer (ITIMER_REAL, &timer, 0) < 0){
	    perror ("Erro em setitimer: ") ;
	    exit (1) ;
    }

    //incializacao de tempo do SO
    tempoTotal = 0;
    tempoInicioTarefa = 0;
    tempoFimTarefa = 0;
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

    //Inicializa as prioridades da tarefa
    task->prioEstatica = 0;
    task->prioEstatica = 0;

    //por padrao nenhuma tarefa eh de sistema
    task->isTarefaSistema = 0;

    //definir o quantum da tarefa
    task->quantum = QUANTUM;

    //definir os tempos da tarefa
    task->tempoProcessamento = 0;
    task->inicioTempoExecucao = systime();
    task->nmroAtivacoes = 0;

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

    //seta o quantum da tarefa a ser executada
    task->quantum = QUANTUM;

    //mudando os tempos das tarefas
    tempoFimTarefa = systime();
    //tarefaAtual->tempoProcessamento += tempoFimTarefa - tempoInicioTarefa;
    task->nmroAtivacoes++;
    tempoInicioTarefa = systime();

    #ifdef DEBUG
		printf("task_switch: mudou a tarefa %d para a tarefa %d\n", temp->tid, tarefaAtual->tid);
	#endif
    
	//troca efetivamente o contexto
	swapcontext(&(temp->Contexto), &(tarefaAtual->Contexto));

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

    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n",
	    tarefaAtual->tid, 
        systime() - tarefaAtual->inicioTempoExecucao,
        tarefaAtual->tempoProcessamento, 
        tarefaAtual->nmroAtivacoes);

    if(tarefaAtual->tid != tarefaDespachante.tid){
        //retorna para o despachante
        task_switch(&tarefaDespachante);
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
        proxima = escalonamento();
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

    if(tarefaAtual->tid != tarefaDespachante.tid){
        //Pega a tarefa atual e coloca ela na fina novamente caso não tenha sido finalizada ainda
        queue_append((queue_t **) &tarefasProntas, (queue_t *) tarefaAtual);
    }

    task_switch(&tarefaDespachante);

}

void task_suspend (task_t *task, task_t **queue){
    if(queue != NULL){
        if(task != NULL){
            queue_t *suspensa;
            //isola a tarefa selecionada
            suspensa = queue_remove((queue_t **) &queue, (queue_t *) task);

            //Adiciona a tarefa a fila de suspensas
            queue_append((queue_t **) &tarefasSuspensas, suspensa);
        }
    }
}

void task_resume (task_t *task){
    if(task != NULL){

        queue_t *suspensa;
        //isola a tarefa selecionada
        suspensa = queue_remove((queue_t **) &tarefasSuspensas, (queue_t *) task);

        //Adiciona a tarefa a fila de prontas
        queue_append((queue_t **) &tarefasProntas, suspensa);
    }
}

//Essa função deve retornar a tarefa de maior prioridade
task_t *escalonamento(){
    //Verifica se a lista existe
    if(tarefasProntas != NULL){

        task_t *atual, *menorValorPrio;

        //Inicializa o atual e o menor sendo o menorValorPrio inicializado com o primeiro elemento da lista e o atual o segundo
        atual = tarefasProntas;
        atual = atual->next;
        menorValorPrio = tarefasProntas;
        //Loop para percorrer toda a lista
        while(atual != tarefasProntas){

            if(atual->prioDinamica < menorValorPrio->prioDinamica){
                //Caso exista um de valor menor que o que se apresenta atualmente ele passa a ser o menorValorPrio
                menorValorPrio = atual;

            }else if(atual->prioDinamica == menorValorPrio->prioDinamica){//caso exista um empate

                if(atual->prioEstatica < menorValorPrio->prioEstatica){//o desempate é feito com base a prioridade estática de cada elemento
                    menorValorPrio = atual;
                }
            }

            atual = atual->next;
        }
        //Envelhecimento de todas as tarefas na fila de prontas
        envelhecimento();
        
        //Reinicia a prioridade dinamica da tarefa;
        menorValorPrio->prioDinamica = menorValorPrio->prioEstatica;
        return menorValorPrio;

    }else{
        return NULL;
    }

}

int task_getprio (task_t *task){

    return tarefaAtual->prioEstatica;

}

void task_setprio (task_t *task, int prio){
    //Verifica se a tarefa existe
    if(task != NULL){
        //Define as duas prioridades inicialmente como a prioridade passada
        task->prioDinamica = prio;
        task->prioEstatica = prio;
    }
}

void envelhecimento(){
    //Verifica se lista existe
    if(tarefasProntas != NULL){
        //Inicialização da váriavel que tem a tarefa a ser alterada;
        task_t *atual;
        atual = tarefasProntas;
        //Loop para percorrer lista
        do{
            atual->prioDinamica = atual->prioDinamica + ALFA;

            if(atual->prioDinamica < -20){
                atual->prioDinamica = -20;
            }

            atual = atual->next;

        }while(atual != tarefasProntas);
    }

}

//retorna o tempo total que o SO está sendo executado
unsigned int systime(){
    return(tempoTotal);
}
