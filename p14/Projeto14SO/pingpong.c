// PingPongOS
// Prof. Marco Aurelio, DAINF UTFPR
//
// Marcelo Guimar�es da Costa
// Pedro Henrique Woiciechovski

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include "pingpong.h"
#include "harddisk.h"
#include "diskdriver.h"
#include "queue.h"

#define STACKSIZE 32768

#define DEFAULT_PRIO 0
#define MIN_PRIO -20
#define MAX_PRIO 20
#define ALPHA_PRIO 1

#define RESET_TICKS 10
#define TICK_MICROSECONDS 1000

//Criando o disco virtual
disk_t disco;

// ponteiros e tarefas not�veis
task_t tarefaMain, *tarefaAtual, tarefaDispatcher, *tarefaLiberar, tarefaDisco;

// filas de tarefas
task_t *filaPronta, *filaSoneca;

// ID da pr�xima task a ser criada
long idProximaTarefa;

// Contagem de tasks de usu�rio criadas
long countTasks;

// Flag que indica se a preempcao por tempo esta ativa ou nao
unsigned char FLAG_TSL;

// Preemp��o por tempo
void CLOCK();
short ticksRestantesTarefaAtual;
struct sigaction strAcao;
struct itimerval strTimer;

//Captura do sinal SIGUSR1
struct sigaction sigDiscoAcao;

// Rel�gio do sistema
unsigned int relogioSistema;

// Fun��o a ser executada pela task do dispatcher
void body_dispatcher(void* arg);

// Fun��o que retorna a pr�xima task a ser executada.
task_t* body_escalonador();

void pingpong_init() {
    // Desativa o buffer de sa�da padr�o
    setvbuf(stdout, 0, _IONBF, 0);

    filaPronta = NULL;
    filaSoneca = NULL;

    // INICIA A TASK MAIN
    // Refer�ncia a si mesmo
    tarefaMain.main = &tarefaMain;

    // A task main esta pronta.
    tarefaMain.estado = 'r';

    // A task main tem id 0.
    tarefaMain.tid = 0;

    // Informa��es de tempo
    tarefaMain.momentoCriacao = systime();
    tarefaMain.momentoUltimaExecucao = 0;
    tarefaMain.tempoExecucao = 0;
    tarefaMain.tempoProcessando = 0;
    tarefaMain.nmroAtivacoes = 0;
    FLAG_TSL = 1;

    tarefaMain.filaJoin = NULL;

    tarefaMain.tempoSoneca = 0;

    // Coloca a tarefa na fila
    queue_append((queue_t**)&filaPronta, (queue_t*)&tarefaMain);
    tarefaMain.fila = &filaPronta;

    // O id da pr�xima task a ser criada � 1.
    idProximaTarefa = 1;

    // A contagem de tasks de usu�rio inicia em 0.
    countTasks = 0;

    // A task que est� executando nesse momento � a main (que chamou pingpong_init).
    tarefaAtual = &tarefaMain;

    // Nao ha nenhuma task para ser liberada.
    tarefaLiberar = NULL;

    // Preemp��o por tempo
    ticksRestantesTarefaAtual = RESET_TICKS;
    strAcao.sa_handler = CLOCK;
    sigemptyset(&strAcao.sa_mask);
    strAcao.sa_flags = 0;
    if (sigaction(SIGALRM, &strAcao, 0) < 0) {
        perror("Erro em sigaction: ");
        exit(1);
    }
    strTimer.it_value.tv_usec = TICK_MICROSECONDS;
    strTimer.it_value.tv_sec = 0;
    strTimer.it_interval.tv_usec = TICK_MICROSECONDS;
    strTimer.it_interval.tv_sec = 0;
    if (setitimer(ITIMER_REAL, &strTimer, 0) < 0) {
        perror("Erro em setitimer: ");
        exit(1);
    }
    relogioSistema = 0;

    // O contexto n�o precisa ser salvo agora, porque a primeira troca de contexto far� isso.

    // INICIA A TASK body_escalonador
    task_create(&tarefaDispatcher, &body_dispatcher, NULL);
    queue_remove((queue_t**)&filaPronta, (queue_t*)&tarefaDispatcher);

    //Inicializa a sigDiscoAcao
    sigDiscoAcao.sa_handler = handlerDisco;
    sigemptyset(&sigDiscoAcao.sa_mask);
    sigDiscoAcao.sa_flags = 0;
    if(sigaction(SIGUSR1, &sigDiscoAcao, 0) < 0){
        perror("Erro em sigaction: ");
        exit(1);
    }

    //Inicializa tarefa de disco
    task_create(&tarefaDisco, &diskDriverBody, NULL);
    countTasks--;

    // Ativa o dispatcher
    task_yield();
}

int task_create(task_t* task, void(*start_func)(void*), void* arg) {
    char* stack;

    // Coloca refer�ncia para task main.
    task->main = &tarefaMain;

    // Inicializa o contexto.
    getcontext(&(task->Contexto));

    // Aloca a pilha.
    stack = malloc(STACKSIZE);
    if (stack == NULL) {
        perror("Erro na cria��o da pilha: ");
        return -1;
    }

    // Seta a pilha do contexto.
    task->Contexto.uc_stack.ss_sp = stack;
    task->Contexto.uc_stack.ss_size = STACKSIZE;
    task->Contexto.uc_stack.ss_flags = 0;

    // N�o liga o contexto a outro.
    task->Contexto.uc_link = NULL;

    // Cria o contexto com a fun��o.
    makecontext(&(task->Contexto), (void(*)(void))start_func, 1, arg);

    // Seta o id da task.
    task->tid = idProximaTarefa;
    idProximaTarefa++;

    countTasks++;

    // Informa��es da fila.
    queue_append((queue_t**)&filaPronta, (queue_t*)task);
    task->fila = &filaPronta;
    task->estado = 'r';
    task->prioEstatica = DEFAULT_PRIO;
    task->prioDinamica = task->prioEstatica;

    // Informa��es de tempo
    task->momentoCriacao = systime();
    task->momentoUltimaExecucao = 0;
    task->tempoExecucao = 0;
    task->tempoProcessando = 0;
    task->nmroAtivacoes = 0;

    task->filaJoin = NULL;

    task->tempoSoneca = 0;

    return (task->tid);
}

void task_exit(int exitCode) {
    tarefaLiberar = tarefaAtual;
    tarefaLiberar->estado = 'x';
    tarefaLiberar->codigoSaida = exitCode;

    // Acorda todas as tarefas na fila de join.
    while (tarefaLiberar->filaJoin != NULL) {
        task_resume(tarefaLiberar->filaJoin);
    }

    tarefaLiberar->tempoProcessando += systime() - tarefaLiberar->momentoUltimaExecucao;
    tarefaLiberar->tempoExecucao = systime() - tarefaLiberar->momentoCriacao;
    printf("Task %d exit: execution time %d ms, processor time %d ms, %d activations\n", tarefaLiberar->tid, tarefaLiberar->tempoExecucao, tarefaLiberar->tempoProcessando, tarefaLiberar->nmroAtivacoes);

    countTasks--;

    if (tarefaAtual == &tarefaDispatcher) {
        task_switch(&tarefaMain);
    }
    else {
        task_switch(&tarefaDispatcher);
    }
}

int task_switch(task_t *task) {
    task_t* prevTask;

    prevTask = tarefaAtual;
    tarefaAtual = task;

    prevTask->tempoProcessando += systime() - prevTask->momentoUltimaExecucao;

    task->nmroAtivacoes++;
    task->momentoUltimaExecucao = systime();

#ifdef DEBUG
    printf("task_switch: trocando task %d -> %d.\n", prevTask->tid, task->tid);
#endif

    if (swapcontext(&(prevTask->Contexto), &(task->Contexto)) < 0) {
        perror("Erro na troca de contexto: ");
        tarefaAtual = prevTask;
        return -1;
    }

    return 0;
}

int task_id() {
    return (tarefaAtual->tid);
}

void task_suspend(task_t *task, task_t **queue) {
    // Se task for nulo, considera a tarefa corrente.
    if (task == NULL) {
        task = tarefaAtual;
    }

    // Se queue for nulo, n�o retira a tarefa da fila atual.
    if (queue != NULL) {
        if (task->fila != NULL) {
            queue_remove((queue_t**)(task->fila), (queue_t*)task);
        }
        queue_append((queue_t**)queue, (queue_t*)task);
        task->fila = queue;
    }
#ifdef DEBUG
    printf("task_suspend: tarefa %d suspensa.\n", task->tid);
#endif

    task->estado = 's';
}

void task_resume(task_t *task) {
    // Remove a task de sua fila atual e coloca-a na fila de tasks prontas.
    if (task->fila != NULL) {
        queue_remove((queue_t**)(task->fila), (queue_t*)task);
    }

    queue_append((queue_t**)&filaPronta, (queue_t*)task);
    task->fila = &filaPronta;
    task->estado = 'r';
}

void task_yield() {
    if (tarefaAtual->estado != 's') {
        // Recoloca a task no final da fila de prontas
        queue_append((queue_t**)&filaPronta, (queue_t*)tarefaAtual);
        tarefaAtual->fila = &filaPronta;
        tarefaAtual->estado = 'r';
    }

    // Volta o controle para o dispatcher.
    task_switch(&tarefaDispatcher);
}

void task_setprio(task_t* task, int prio) {
    if (task == NULL) {
        task = tarefaAtual;
    }
    if (prio <= MAX_PRIO && prio >= MIN_PRIO) {
        task->prioEstatica = prio;
        task->prioDinamica = prio;
    }
}

int task_getprio(task_t* task) {
    if (task == NULL) {
        task = tarefaAtual;
    }
    return task->prioEstatica;
}

int task_join(task_t* task) {
    if (task == NULL) {
        return -1;
    }
    if (task->estado == 'x') {
        return task->codigoSaida;
    }

    // Se a tarefa existir e n�o tiver terminado
    FLAG_TSL = 0;
    task_suspend(NULL, &(task->filaJoin));
    FLAG_TSL = 1;

    task_yield();
    return task->codigoSaida;
}

void task_sleep(int t) {
    if(t > 0) {
        tarefaAtual->tempoSoneca = systime() + t*1000; // systime() � em milissegundos.

        FLAG_TSL = 0;
        task_suspend(NULL, &filaSoneca);
        FLAG_TSL = 1;

        task_yield(); // Volta para o dispatcher.
    }
}

void body_dispatcher(void* arg) {
    task_t* iterator;
    task_t* awake;
    unsigned int time;

    while (countTasks > 0) {
        if(filaPronta != NULL) {
            task_t* next = body_escalonador();

            if (next != NULL) {
                // Coloca a tarefa em execu��o
                // Reseta as ticks
                ticksRestantesTarefaAtual = RESET_TICKS;
                queue_remove((queue_t**)&filaPronta, (queue_t*)next);
                next->fila = NULL;
                next->estado = 'e';
                task_switch(next);

                // Libera a memoria da task, caso ela tenha dado exit.
                if (tarefaLiberar != NULL) {
                    free(tarefaLiberar->Contexto.uc_stack.ss_sp);
                    tarefaLiberar = NULL;
                }
            }
        }

        // Percorre a fila de tasks dormindo e acorda as tasks que devem ser acordadas.
        if (filaSoneca != NULL) {
            iterator = filaSoneca;
            time = systime();
            do {
                if(iterator->tempoSoneca <= time) {
                    awake = iterator;
                    iterator = iterator->next;
                    task_resume(awake);
                }
                else {
                    iterator = iterator->next;
                }
            } while (iterator != filaSoneca && filaSoneca != NULL);
        }
    }
    task_exit(0);
}

task_t* body_escalonador() {
    task_t* iterator;
    task_t* nextTask;
    int minDynPrio;
    int minPrio;

    iterator = filaPronta;
    nextTask = NULL;
    minDynPrio = MAX_PRIO + 1;
    minPrio = MAX_PRIO + 1;

    // Se a fila estiver vazia, retorna NULL.
    if (iterator == NULL) {
        return NULL;
    }

    // Busca a tarefa com menor dynPrio para executar.
    do {
        if (iterator->prioDinamica < minDynPrio) {
            nextTask = iterator;
            minDynPrio = iterator->prioDinamica;
            minPrio = iterator->prioEstatica;
        }
        else if (iterator->prioDinamica == minDynPrio) { // Desempate
            if (iterator->prioEstatica < minPrio) {
                nextTask = iterator;
                minDynPrio = iterator->prioDinamica;
                minPrio = iterator->prioEstatica;
            }
        }

        iterator = iterator->next;
    } while (iterator != filaPronta);

    // Retira a tarefa da fila e reseta sua prioridade dinamica.
    nextTask->prioDinamica = nextTask->prioEstatica;
    nextTask->prioDinamica += ALPHA_PRIO; // Para n�o precisar verificar se cada outra task � a nextTask ou n�o.

    // Atualiza a dynprio das outras tarefas.
    iterator = filaPronta;
    if (iterator != NULL) {
        do {
            iterator->prioDinamica -= ALPHA_PRIO;
            iterator = iterator->next;
        } while (iterator != filaPronta);
    }

    return nextTask;
}

void CLOCK() {
    relogioSistema++;

    if (tarefaAtual != &tarefaDispatcher) {
        ticksRestantesTarefaAtual--;

        if (FLAG_TSL && ticksRestantesTarefaAtual <= 0) {
            task_yield();
        }
    }
}

unsigned int systime() {
    return relogioSistema * TICK_MICROSECONDS / 1000;
}


//-----------------------------SEMAFORO-------------------------------------

int sem_create(semaphore_t* s, int valor) {
    if (s == NULL) {
        return -1;
    }

    FLAG_TSL = 0;
    s->fila = NULL;
    s->valor = valor;
    s->isAtivo = 1;

#ifdef DEBUG
    printf("sem_create: criado semaforo com valor inicial %d.\n", valor);
#endif

    FLAG_TSL = 1;
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }

    return 0;
}

int sem_down(semaphore_t* s) {
    if (s == NULL || !(s->isAtivo)) {
        return -1;
    }

    FLAG_TSL = 0;

    if (s->valor <= 0) {
#ifdef DEBUG
        printf("sem_down: semaforo cheio, suspendendo tarefa %d\n", tarefaAtual->tid);
#endif
        // Caso n�o existam mais vagas no sem�foro, suspende a tarefa.
        task_suspend(tarefaAtual, &(s->fila));

        FLAG_TSL = 1;
        task_yield();

        // Se a tarefa foi acordada devido a um sem_destroy, retorna -1.
        if (!(s->isAtivo)) {
            return -1;
        }

#ifdef DEBUG
        printf("sem_down: semaforo obtido pela tarefa %d\n", tarefaAtual->tid);
#endif


        return 0;
    }else{
        s->valor--;
    }

#ifdef DEBUG
    printf("sem_down: semaforo obtido pela tarefa %d\n", tarefaAtual->tid);
#endif
    FLAG_TSL = 1;
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }
    return 0;
}

int sem_up(semaphore_t* s) {
    if (s == NULL || !(s->isAtivo)) {
        return -1;
    }

    FLAG_TSL = 0;

    if (s->fila != NULL) {
        task_resume(s->fila);
    }else{
        s->valor++;
    }
    FLAG_TSL = 1;

#ifdef DEBUG
    printf("sem_up: semaforo liberado pela tarefa %d\n", tarefaAtual->tid);
#endif
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }
    return 0;
}

int sem_destroy(semaphore_t* s) {
    if (s == NULL || !(s->isAtivo)) {
        return -1;
    }

    FLAG_TSL = 0;
    s->isAtivo = 0;
    while (s->fila != NULL) {
        task_resume(s->fila);
    }

    FLAG_TSL = 1;
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }
    return 0;
}

//-----------------------------BARRIER-------------------------------------

int barrier_create(barrier_t* b, int N) {
    if (b == NULL || N <= 0) {
        return -1;
    }

    FLAG_TSL = 0;
    b->tarefasMaximo = N;
    b->tarefasContagem = 0;
    b->isAtivo = 1;

    FLAG_TSL = 1;
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }
    return 0;
}

int barrier_join(barrier_t* b) {
    if (b == NULL || !(b->isAtivo)) {
        return -1;
    }

    FLAG_TSL = 0;
    b->tarefasContagem++;

    if (b->tarefasContagem == b->tarefasMaximo) {
        while (b->fila != NULL) {
            task_resume(b->fila);
        }
        b->tarefasContagem = 0;
        FLAG_TSL = 1;
        if(ticksRestantesTarefaAtual <= 0) {
            task_yield();
        }
        return 0;
    }

    task_suspend(tarefaAtual, &(b->fila));
    FLAG_TSL = 1;
    task_yield();

    if(!(b->isAtivo)) {
        return -1;
    }
    return 0;
}

int barrier_destroy(barrier_t* b) {
    if (b == NULL || !(b->isAtivo)) {
        return -1;
    }

    FLAG_TSL = 0;
    b->isAtivo = 0;
    while (b->fila != NULL) {
        task_resume(b->fila);
    }

    FLAG_TSL = 1;
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }
    return 0;
}

//-----------------------------MENSAGENS-------------------------------------

//cria a fila de mensagens
int mqueue_create(mqueue_t* queue, int max, int size) {
    if(queue == NULL) {
        return -1;
    }

    FLAG_TSL = 0;

	//aloca o conteudo
    queue->content = malloc(max * size);

	//define o tamanho
	queue->mensagemTamanho = size;

	//define o maximo de mensagens
	queue->mensagensTamanho = max;

	//inicia com 0 mensagens
    queue->mensagensContador = 0;

	//cria os semaforos para o conteudo

	//buffer fica bloqueado enquanto esta sendo escrito
    sem_create(&(queue->semaforoBuffer), 1);

	//?
    sem_create(&(queue->semaforoItem), 0);

	//coloca em fila de espera quando for solicitada uma vaga na lista quando cheia
	sem_create(&(queue->semaforoVaga), max);

    queue->isAtivo = 1;

    FLAG_TSL = 1;

	//perde o processador se acabou o quantum
    if(ticksRestantesTarefaAtual <= 0) {
        task_yield();
    }
    return 0;
}

//funcao para mandar mensagem
int mqueue_send(mqueue_t* queue, void* msg) {
#ifdef DEBUG
    int i;
#endif

	//condi��es de erro
    if (queue == NULL
	|| !(queue->isAtivo)
	|| sem_down(&(queue->semaforoVaga)) == -1
	|| sem_down(&(queue->semaforoBuffer))) {
        return -1;
    }

	//acessa a posicao de memoria e copia a mensagem para o buffer
    memcpy(queue->content + queue->mensagensContador * queue->mensagemTamanho, msg, queue->mensagemTamanho);
    ++(queue->mensagensContador);

#ifdef DEBUG
    printf("mqueue_send: ");
    for (i = 0; i < queue->mensagensContador; i++) {
        printf("%d ",((int*)(queue->content))[i]);
    }
    printf("\n");
#endif

	//libera os semaforos
    sem_up(&(queue->semaforoBuffer));
    sem_up(&(queue->semaforoItem));

    return 0;
}

//funcao para receber a mensagem
int mqueue_recv(mqueue_t* queue, void* msg) {
    int i;
    void* destination;
    void* source;

	//condicoes de erro
    if (queue == NULL || !(queue->isAtivo)) {
        return -1;
    }

	//se der certo requisitar o buffer e o item
    if (sem_down(&(queue->semaforoItem)) == -1) return -1;
    if (sem_down(&(queue->semaforoBuffer)) == -1) return -1;

#ifdef DEBUG
    printf("mqueue_recv: ");
    for (i = 0; i < queue->mensagensContador; i++) {
        printf("%d ",((int*)(queue->content))[i]);
    }
    printf("\n");
#endif

	//decrementa fila e recebe mensage
    --(queue->mensagensContador);
    memcpy(msg, queue->content, queue->mensagemTamanho);

	//copia os itens de volta para o come�o da fila
    source = queue->content + queue->mensagemTamanho;
    destination = queue->content;
    for (i = 0; i < queue->mensagensContador; i++) {
        memcpy(destination, source, queue->mensagemTamanho);
        source += queue->mensagemTamanho;
        destination += queue->mensagemTamanho;
    }

#ifdef DEBUG
    printf("mqueue_recv: ");
    for (i = 0; i < queue->mensagensContador; i++) {
        printf("%d ",((int*)(queue->content))[i]);
    }
    printf("\n");
#endif

	//libera os semaforos
    sem_up(&(queue->semaforoBuffer));
    sem_up(&(queue->semaforoVaga));

    return 0;
}

//desaloca todos os elementos
int mqueue_destroy(mqueue_t* queue) {
    if (queue == NULL || !(queue->isAtivo)) {
        return -1;
    }

    queue->isAtivo = 0;
    free(queue->content);
    sem_destroy(&(queue->semaforoBuffer));
    sem_destroy(&(queue->semaforoItem));
    sem_destroy(&(queue->semaforoVaga));

    return 0;
}

//retorna a quantidade de mensagens presentes na fila
int mqueue_msgs(mqueue_t* queue) {
    if (queue == NULL || !(queue->isAtivo)) {
        return -1;
    }

    return queue->mensagensContador;
}

/*-------------------------------------DISCO---------------------------------------------------*/

void diskDriverBody(void * args){

    discoPedido *pedidoAtual;
    task_t *tarefaProxima;

    while(1){
        //Faz uma chamada e verifica se ocorreu bem
        if(sem_down(&(disco.semDisco)) < 0){
            return -1;
        }

        //Se o disco foi acordado por um sinal
        if(disco.isSinal == 1){
            //Desativa o sinal, faz resume da primeira tarefa da lista e informa que o disco est� ocupado
            disco.isSinal = 0;
            tarefaProxima = escalonamentoTarefaSSTF();
            task_resume(pedidoAtual->tarefa);
            disco.isLivre = 1;
        }

        //Se o disco est� livre e possui pedidos esperando
        if(disco.isLivre == 1 && disco.filaPedidos != NULL){
            //Remove o primeiro pedido da fila
            pedidoAtual = escalonamentoPedidoSSTF();
            disco.blocoAtual = pedidoAtual->bloco;

            //Se for leitura
            if(pedidoAtual->escrever == 0){
                disk_cmd(DISK_CMD_READ, pedidoAtual->bloco, pedidoAtual->buffer);
                disco.isLivre = 0;
            }else{//Se for escrita
                disk_cmd(DISK_CMD_WRITE, pedidoAtual->bloco, pedidoAtual->buffer);
                disco.isLivre = 0;
            }
        }

        //Faz uma chamada e verifica se ocorreu bem
        if(sem_up(&(disco.semDisco)) < 0){
            return -1;
        }

        task_yield();
    }

}

int diskdriver_init (int *numBlocks, int *blockSize){

    if (disk_cmd(DISK_CMD_INIT, 0, NULL) < 0) {
        return -1;
    }
    //Pega o n�mero de blocos e o tamanho de cada bloco
    *numBlocks = disk_cmd(DISK_CMD_DISKSIZE, 0, NULL);
    *blockSize = disk_cmd(DISK_CMD_BLOCKSIZE, 0, NULL);

    //Caso tenha ocorrido erro em um deles
    if(*numBlocks < 0 || *blockSize < 0){
        return -1;
    }

    //Inicializa o disco
    disco.tamanhoDisco = *numBlocks;
    disco.tamanhoBloco = *blockSize;
    disco.isLivre = 1;
    disco.isSinal = 0;
    disco.filaPedidos = NULL;
    disco.filaEspera = NULL;
    disco.blocoAtual = 0;

    //Inicializa o semaforo do disco
    if(sem_create(&(disco.semDisco), 1)){
        return -1;
    }

    return 0;

}

int disk_block_read (int block, void *buffer){

    discoPedido *pedidoLeitura;

    //Faz uma chamada e verifica se ocorreu bem
    if(sem_down(&(disco.semDisco)) < 0){
        return -1;
    }
    //Inicializa pedidoLeitura
    pedidoLeitura = malloc(sizeof(discoPedido));
    pedidoLeitura->bloco = block;
    pedidoLeitura->buffer = buffer;
    pedidoLeitura->tarefa = tarefaAtual;

    pedidoLeitura->escrever = 0;

    pedidoLeitura->next = NULL;
    pedidoLeitura->prev = NULL;

    queue_append((queue_t **)&(disco.filaPedidos), (queue_t *)pedidoLeitura);

    //Se a tarefa de disco estiver suspensa
    if(tarefaDisco.estado == 's'){
        //Coloca ela na fila de prontas
        task_resume(&tarefaDisco);
    }

    //Faz uma chamada e verifica se ocorreu bem
    if(sem_up(&(disco.semDisco)) < 0){
        return -1;
    }

    task_suspend(NULL, &(disco.filaEspera));
    task_yield();

    return 0;
}

int disk_block_write (int block, void *buffer){

    discoPedido *pedidoEscritura;

    //Faz uma chamada e verifica se ocorreu bem
    if(sem_down(&(disco.semDisco)) < 0){
        return -1;
    }
    //Inicializa pedidoLeitura
    pedidoEscritura = malloc(sizeof(discoPedido));
    pedidoEscritura->bloco = block;
    pedidoEscritura->buffer = buffer;
    pedidoEscritura->tarefa = tarefaAtual;

    pedidoEscritura->escrever = 1;

    pedidoEscritura->next = NULL;
    pedidoEscritura->prev = NULL;

    queue_append((queue_t **)&(disco.filaPedidos), (queue_t *)pedidoEscritura);

    //Se a tarefa de disco estiver suspensa
    if(tarefaDisco.estado == 's'){
        //Coloca ela na fila de prontas
        task_resume(&tarefaDisco);
    }

    //Faz uma chamada e verifica se ocorreu bem
    if(sem_up(&(disco.semDisco)) < 0){
        return -1;
    }

    task_suspend(NULL, &(disco.filaEspera));
    task_yield();

    return 0;

}

void handlerDisco(int signum){
    // Muda o disco para acordado por um sinal;
    disco.isSinal = 1;
}

/*-----------------------ESCALONAMENTO DE DISCO-----------------------*/

//para int a��o 0 retornar o pedido, 1 remover o pedido

discoPedido* escalonamentoPedidoFCFS(){

    //Remove o primeiro pedido da fila e retorna;
    return (discoPedido *)queue_remove((queue_t **)&(disco.filaPedidos), (queue_t *)disco.filaPedidos);

}

task_t* escalonamentoTarefaFCFS(){
    //Retorna a tarefa de pedido
    if(disco.filaPedidos != NULL){
        return disco.filaPedidos->tarefa;
    }else{
        return NULL;
    }
}

discoPedido* escalonamentoPedidoSSTF(){
    //Verifica se existe fila de pedidos
    if(disco.filaPedidos == NULL){
        return NULL;
    }

    //Variavel da menor diferen�a de blocos
    int menorDiferenca=1000;
    //Pedido sendo avaliado agora
    discoPedido *pedidoAtual;
    //Pedido a ser retornado
    discoPedido *menorDistPedido;

    pedidoAtual = disco.filaPedidos;

    //Loop para buscar menor distancia
    do{
        //Caso a diferen�a seja menor, altera o menor pedido
        if(menorDiferenca > abs(pedidoAtual->bloco - disco.blocoAtual)){
            menorDiferenca = abs(pedidoAtual->bloco - disco.blocoAtual);
            menorDistPedido = pedidoAtual;
        }

        //Muda para o pr�ximo pedido
        pedidoAtual = pedidoAtual->next;

    }while(pedidoAtual != disco.filaPedidos);

    return (discoPedido *)queue_remove((queue_t **)&(disco.filaPedidos), (queue_t *)menorDistPedido);

}

task_t* escalonamentoTarefaSSTF(){

    //Verifica se existe fila de pedidos
    if(disco.filaPedidos == NULL){
        return NULL;
    }

    //Variavel da menor diferen�a de blocos
    int menorDiferenca=1000;
    //Pedido sendo avaliado agora
    discoPedido *pedidoAtual;
    //Pedido a ser retornado
    discoPedido *menorDistPedido;

    pedidoAtual = disco.filaPedidos;

    //Loop para buscar menor distancia
    do{
        //Caso a diferen�a seja menor, altera o menor pedido
        if(menorDiferenca > abs(pedidoAtual->bloco - disco.blocoAtual)){
            menorDiferenca = abs(pedidoAtual->bloco - disco.blocoAtual);
            menorDistPedido = pedidoAtual;
        }

        //Muda para o pr�ximo pedido
        pedidoAtual = pedidoAtual->next;

    }while(pedidoAtual != disco.filaPedidos);

    return menorDistPedido->tarefa;

}

discoPedido* escalonamentoPedidoCSCAN(){

        //Verifica se existe fila de pedidos
    if(disco.filaPedidos == NULL){
        return NULL;
    }

    //Variavel da menor diferen�a de blocos
    int menorDiferenca=1000;
    //Pedido sendo avaliado agora
    discoPedido *pedidoAtual;
    //Pedido a ser retornado
    discoPedido *menorDistPedido;

    pedidoAtual = disco.filaPedidos;

    do{
        //Caso a diferen�a seja menor, altera o menor pedido
        if(menorDiferenca > abs(pedidoAtual->bloco - disco.blocoAtual)){
            menorDiferenca = abs(pedidoAtual->bloco - disco.blocoAtual);
            menorDistPedido = pedidoAtual;
        }

        //Muda para o pr�ximo pedido
        pedidoAtual = pedidoAtual->next;

    }while(pedidoAtual != disco.filaPedidos);

}

task_t* escalonamentoTarefaCSCAN(){}
