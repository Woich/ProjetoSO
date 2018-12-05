#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "pingpong.h"
#include "queue.h"
#include "harddisk.h"
#include "diskdriver.h"

#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#define STACKSIZE 32768
#define _XOPEN_SOURCE 600


int id = 0, userTasks = 0, tempo = 0, usingprio = 0;
task_t mainTask, diskTask, *taskAtual, dispatcher, *taskQueue, *suspendedTasks, *sleepingTasks;

struct sigaction action;
struct sigaction disk_action;
struct itimerval timer;

disk_t disk;

unsigned int systime(){
    return tempo;
}

void printQueue(){
    task_t *iter = taskQueue;
    printf("Queue: ");
    while (iter->next != taskQueue && iter->next != NULL){
        printf(" %d<-%d->%d ",iter->prev->tid, iter->tid, iter->next->tid);
        iter=iter->next;
    }
    printf(" %d<-%d->%d ",iter->prev->tid, iter->tid, iter->next->tid);
    printf("\n");
}

void task_sleep(int t){
    taskAtual->sleep_wake = systime()+t*1000;
    queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
    queue_append((queue_t**) &sleepingTasks, (queue_t*) taskAtual);
    task_yield();
}

void preemp_handler(int signum){
    if (signum == 14 && taskAtual != &dispatcher){
        if (taskAtual->quantum > 0){
            taskAtual->quantum--;
            taskAtual->cpu_time++;
        } else {
            task_yield();
        }
    }
    if (signum == 14){
        tempo++;
    }
}

void pingpong_init(){
    setvbuf(stdout, 0, _IONBF, 0);
    mainTask.prev = NULL;
    mainTask.next = NULL;
    mainTask.tid = id;
    taskAtual = &mainTask;

    action.sa_handler = preemp_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction (SIGALRM, &action, 0) < 0){
        perror ("Erro em sigaction: ");
        exit (1);
    }

    disk_action.sa_handler = diskact_handler;
    sigemptyset(&disk_action.sa_mask);
    disk_action.sa_flags = 0;
    if (sigaction (SIGUSR1, &disk_action, 0) < 0){
        perror ("Erro em sigaction: ");
        exit (1);
    }

    timer.it_value.tv_usec = 1000;
    timer.it_interval.tv_usec = 1000;

    if (setitimer (ITIMER_REAL, &timer, 0) < 0){
        perror ("Erro em setitimer: ");
        exit (1);
    }
    queue_append((queue_t**) &taskQueue, (queue_t*) &mainTask);
    userTasks++;

    task_create(&dispatcher, (void*) dispatcher_body, NULL);

    task_create(&diskTask, (void*) diskDriverBody, NULL);
    userTasks--;
}

task_t* scheduler(){
    if (usingprio == 0){
        if (taskQueue == NULL)
            return NULL;
        taskQueue = taskQueue->next;
        return taskQueue;
    } else {
        task_t *iter = taskQueue, *max = taskQueue;
        while (iter->next != taskQueue){
            if (iter->prio < max->prio){
                max = iter;
            }
            iter = iter->next;
        }
        if (iter->prio < max->prio){
            max = iter;
        }
        //printf("TID PRIO %d %d\n", max->tid, max->prio);
        return max;
    }
}

int task_join (task_t *task) {
    if (find_task(task) == -1)
        return -1;
    else {
        taskAtual->waiting = task;

        queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
        queue_append((queue_t**) &suspendedTasks, (queue_t*) taskAtual);

        task_yield();
    }
    return taskAtual->join_exitcode;
}

int queue_count(task_t *queue){
    int cnt = 0;
    task_t *itr = queue;

    if (queue == NULL){
        return 0;
    } else {
        cnt++;
        while (itr->next != queue){
            cnt++;
            itr = itr->next;
        }
        return cnt;
    }
}

int find_task (task_t *task){
    task_t *itr = taskQueue;
    int tqcnt = queue_count(taskQueue), i;

    for (i=0; i<tqcnt; i++){
        if (itr == task) return 1;
        itr = itr->next;
    }
    return -1;
}

void task_setprio (task_t *task, int prio) {
    usingprio = 1;
    if (prio > 20 || prio < -20) {
        printf("Erro: task_setprio precisa ter como entrada uma prioridade entre -20 e 20");
        exit(0);
    }
    if (task == NULL){
        taskAtual->prio = prio;
        taskAtual->initprio = prio;
    }
    else {
        task->prio = prio;
        task->initprio = prio;
    }
}

int task_getprio (task_t *task){
    if (task == NULL) return taskAtual->prio;
    return task->prio;
}

void age(task_t *task){
    task_t *iter = taskQueue;
    if (iter != NULL){
        while (iter->next != taskQueue){
            if (iter != task && iter->prio < 20 && iter->prio > -20) iter->prio--;
            iter = iter->next;
        }
    }
    if (iter != NULL && iter != task && iter->prio < 20 && iter->prio > -20) iter->prio--;
    task->prio = task->initprio;
}

int task_create (task_t *task, void (*start_routine)(void *), void *arg){
    getcontext (&(task->context));

    char *stack = malloc (STACKSIZE);
    if (stack)
    {
        (task->context).uc_stack.ss_sp = stack ;
        (task->context).uc_stack.ss_size = STACKSIZE;
        (task->context).uc_stack.ss_flags = 0;
        (task->context).uc_link = 0;
    }
    else
    {
        perror ("Erro na criação da pilha: ");
        return -1;
    }

    makecontext (&(task->context), (void*)(*start_routine), 1, arg);
    task->tid = ++id;
    task->prio = 0;
    task->initprio = 0;
    task->quantum = 20;
    task->activations = 0;
    task->init_time = systime();
    task->cpu_time = 0;
    task->waiting = NULL;
    task->join_exitcode = -1;
    task->auxStatus = 0;

    if (task != &dispatcher){
        queue_append((queue_t**) &taskQueue, (queue_t*) task);
        userTasks++;
    }

    #ifdef DEBUG
    printf ("task_create: criou tarefa %d\n", task->tid);
    #endif

    return task->tid;
}

int task_switch (task_t *task){
    task_t *taskAux;
    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n", taskAtual->tid, task->tid);
    #endif
    taskAux = taskAtual;
    taskAtual = task;
    taskAtual->quantum = 20;
    taskAtual->activations++;
    if (swapcontext(&(taskAux->context), &(task->context)) != -1) return 0;
    return -1;
}

void task_exit (int exit_code) {
    #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n", taskAtual->tid);
    #endif
    task_t *itr = suspendedTasks, *cur;
    int i, sutc = queue_count(suspendedTasks);

    userTasks--;
    queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
    taskAtual->auxStatus = 0;
    printf("Task %d exit: running time %d ms, cpu time %d ms, %d activations\n", taskAtual->tid,
        systime()-taskAtual->init_time, taskAtual->cpu_time, taskAtual->activations);

    for (i=0; i<sutc; i++){
        if (itr->waiting == taskAtual){
            cur = itr;
            itr = itr->next;

            cur->waiting = NULL;
            cur->join_exitcode = exit_code;
            queue_remove((queue_t**) &suspendedTasks, (queue_t*) cur);
            queue_append((queue_t**) &taskQueue, (queue_t*) cur);

        } else {
            itr = itr->next;
        }
    }
    task_yield();
   // task_switch(&mainTask);
}

int task_id (){
    return taskAtual->tid;
}

void task_yield(){
    task_switch(&dispatcher);
}

void dispatcher_body(){
    task_t *next, *itr, *cur;
    int slcnt, i;
    while (userTasks > 0){
        slcnt = queue_count(sleepingTasks);
        itr = sleepingTasks;

        for (i=0; i<slcnt; i++){
            if (itr->sleep_wake < systime()){
                cur = itr;
                itr = itr->next;
                queue_remove((queue_t**) &sleepingTasks, (queue_t*) cur);
                queue_append((queue_t**) &taskQueue, (queue_t*) cur);
            } else {
                itr = itr->next;
            }
        }

        next = scheduler();
        if (next) {
            task_switch(next);
            if (usingprio == 1) age(next);
        }
    }
    task_switch(&mainTask);
}

int sem_create(semaphore_t *s, int value){
    s->sem_queue = NULL;
    s->sem_value = value;
    if (s->sem_queue == NULL && s->sem_value == value)
        return 0;
    else
        return -1;
}

int sem_down(semaphore_t *s) {
    if (s == NULL)
        return -1;

    if (s->sem_value <= 0){
        queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
        queue_append((queue_t**) &s->sem_queue, (queue_t*) taskAtual);
        task_yield();
    } else {
        s->sem_value--;
    }
    return 0;
}

int sem_up(semaphore_t *s){
    if (s == NULL)
        return -1;

    if (s->sem_queue != NULL){
        task_t *topqueue = s->sem_queue;
        queue_remove((queue_t**) &s->sem_queue, (queue_t*) topqueue);
        queue_append((queue_t**) &taskQueue, (queue_t*) topqueue);
    } else {
        s->sem_value++;
    }
    return 0;
}

int sem_destroy(semaphore_t *s){
    int sem_cnt = queue_count(s->sem_queue), i;
    task_t *itr = s->sem_queue, *cur;

    for (i=0; i<sem_cnt; i++){
        cur = itr;
        itr = itr->next;
        queue_remove((queue_t**) &s->sem_queue, (queue_t*) cur);
        queue_append((queue_t**) &taskQueue, (queue_t*) cur);
    }

    if (s->sem_queue == NULL)
        return 0;
    return 1;
}

int barrier_create (barrier_t *b, int N){
    b->max_tasks = N;
    b->task_count = 0;
    b->b_queue = NULL;
    if (b->b_queue == NULL && b->max_tasks == N)
        return 0;
    return -1;
}

int barrier_join (barrier_t *b){
    int i;
    task_t *cur, *itr;

    if (b==NULL) return -1;
    if (b->task_count+1 < b->max_tasks){
        queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
        queue_append((queue_t**) &b->b_queue, (queue_t*) taskAtual);
        b->task_count++;
        task_yield();
    } else {
        itr = b->b_queue;
        for (i=0; i<b->max_tasks-1; i++){
            cur = itr;
            itr = itr->next;

            queue_remove((queue_t**) &b->b_queue, (queue_t*) cur);
            queue_append((queue_t**) &taskQueue, (queue_t*) cur);
        }
        b->task_count = 0;
    }
    return 0;
}

int barrier_destroy (barrier_t *b){
    int i;
    task_t *cur, *itr;
    itr = b->b_queue;
    for (i=0; i<b->task_count; i++){
        cur = itr;
        itr = itr->next;

        queue_remove((queue_t**) &b->b_queue, (queue_t*) cur);
        queue_append((queue_t**) &taskQueue, (queue_t*) cur);
    }
    b->task_count = 0;

    if (b->task_count == 0 && b->b_queue == NULL)
        return 0;
    else
        return -1;
}

int mqueue_create (mqueue_t *queue, int max, int size){
    queue->max = max;
    queue->size = size;
    queue->msgs = malloc(max*size);
    queue->count = 0;
    queue->sm_inuse = (semaphore_t*) malloc(sizeof(semaphore_t));
    queue->sm_limit = (semaphore_t*) malloc(sizeof(semaphore_t));
    queue->sm_msgcount = (semaphore_t*) malloc(sizeof(semaphore_t));

    sem_create(queue->sm_inuse, 1);
    sem_create(queue->sm_limit, max);
    sem_create(queue->sm_msgcount, 0);

    if (queue->max != max || queue->size != size || queue->msgs == NULL)
        return -1;
    return 0;
}

int mqueue_send (mqueue_t *queue, void *msg){
    if (queue == NULL || (queue->size == 0 && queue->max == 0)) return -1;

    sem_down(queue->sm_limit);
    sem_down(queue->sm_inuse);

    bcopy(msg, queue->msgs+(queue->size*queue->count), queue->size);
    queue->count++;

    sem_up(queue->sm_msgcount);
    sem_up(queue->sm_inuse);

    return 0;
}

int mqueue_recv (mqueue_t *queue, void *msg){
    if (queue == NULL || (queue->size == 0 && queue->max == 0)) return -1;
    int i;

    sem_down(queue->sm_msgcount);
    sem_down(queue->sm_inuse);

    bcopy(queue->msgs, msg, queue->size);

    for (i = 0; i < queue->size; i++){
        bcopy(queue->msgs+(queue->size*i)+queue->size, queue->msgs+(queue->size*i), queue->size);
    }

    queue->count--;

    sem_up(queue->sm_limit);
    sem_up(queue->sm_inuse);

    return 0;
}

int mqueue_destroy (mqueue_t *queue){
    queue->count = 0;
    queue->max = 0;
    queue->size = 0;

    sem_destroy(queue->sm_inuse);
    sem_destroy(queue->sm_limit);
    sem_destroy(queue->sm_msgcount);

    free(queue->msgs);
    queue->msgs = NULL;
    queue = NULL;

    return 0;
}

int mqueue_msgs (mqueue_t *queue){
    return queue->count;
}

int diskdriver_init (int *numBlocks, int *blockSize){
    int diskSize_aux, blockSize_aux;
    if (disk_cmd(DISK_CMD_INIT, 0, NULL)<0) return -1;
    diskSize_aux = disk_cmd(DISK_CMD_DISKSIZE, 0, NULL);
    blockSize_aux = disk_cmd(DISK_CMD_BLOCKSIZE, 0, NULL);
    *numBlocks = diskSize_aux;
    *blockSize = blockSize_aux;

    disk.d_blockSize = blockSize_aux;
    disk.d_diskSize = diskSize_aux;
    disk.d_reqQueue = NULL;
    disk.d_waitQueue = NULL;
    disk.sig = 0;
    disk.livre = 1;
    disk.sem_disk = (semaphore_t*) malloc(sizeof(semaphore_t));
    sem_create(disk.sem_disk, 1);

    return 0;
}

int disk_block_read (int block, void *buffer){
    disk_req *request = (disk_req*)malloc(sizeof(disk_req));;
    sem_down(disk.sem_disk);
    request->block = block;
    request->next = NULL;
    request->prev = NULL;
    request->reqTask = taskAtual;
    request->rw = 0;
    request->buffer = buffer;

    queue_append((queue_t**)&disk.d_reqQueue, (queue_t*)request);

    if (diskTask.auxStatus == 0){
        queue_append((queue_t**)&taskQueue, (queue_t*)&diskTask);
        diskTask.auxStatus = 1;
    }

    sem_up(disk.sem_disk);

    queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
    queue_append((queue_t**) &disk.d_waitQueue, (queue_t*) taskAtual);
    task_yield();

    return 0;
}

int disk_block_write (int block, void *buffer){
    disk_req *request = (disk_req*)malloc(sizeof(disk_req));
    sem_down(disk.sem_disk);
    request->block = block;
    request->next = NULL;
    request->prev = NULL;
    request->reqTask = taskAtual;
    request->rw = 1;
    request->buffer = buffer;

    queue_append((queue_t**)&disk.d_reqQueue, (queue_t*)request);

    if (diskTask.auxStatus == 0){
        queue_append((queue_t**)&taskQueue, (queue_t*)&diskTask);
        diskTask.auxStatus = 1;
    }

    sem_up(disk.sem_disk);

    queue_remove((queue_t**) &taskQueue, (queue_t*) taskAtual);
    queue_append((queue_t**) &disk.d_waitQueue, (queue_t*) taskAtual);
    task_yield();

    return 0;
}

void diskact_handler(int signum){
    disk.sig = 1;
    queue_append((queue_t**)&taskQueue, (queue_t*)&diskTask);
    diskTask.auxStatus = 1;
}

void diskDriverBody(void* arg){
    disk_req *request;

    while (1) {
        sem_down(disk.sem_disk);

        if (disk.sig == 1){
            disk.sig = 0;
            task_t *task = disk.d_waitQueue;
            queue_remove((queue_t**) &disk.d_waitQueue, (queue_t*) disk.d_waitQueue);
            queue_append((queue_t**) &taskQueue, (queue_t*) task);
            disk.livre = 1;
        }

        if (disk.livre == 1  && disk.d_reqQueue != NULL){
            request = disk.d_reqQueue;
            queue_remove((queue_t**)&disk.d_reqQueue, (queue_t*)disk.d_reqQueue);
            if (request->rw == 0){
                disk_cmd(DISK_CMD_READ, request->block, request->buffer);
                disk.livre = 0;
            } else if (request->rw == 1){
                disk_cmd(DISK_CMD_WRITE, request->block, request->buffer);
                disk.livre = 0;
            }
        }

        sem_up(disk.sem_disk);
        queue_remove((queue_t**) &taskQueue, (queue_t*)&diskTask);
        task_yield();
    }
}
