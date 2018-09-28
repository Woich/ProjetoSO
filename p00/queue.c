#include "queue.h"
#include <stdio.h>


void queue_append (queue_t **queue, queue_t *elem){//(fila enviada, elemento a ser adicionado) respectivamente

    /*queue_t *atual;
    atual = (*queue)->prev;*/

    if(queue == NULL){
        printf("Fila não existe\n");
    }else{
        if(elem->next == NULL && elem->prev == NULL){//verifica se elemento já não pertence a outra lista
            if((*queue) == NULL){//caso a fila não possua elementos adiciona o primeiro
                (*queue) = elem;//adiciona o primeiro elemento da lista
                (*queue)->next = elem;//organiza as referencia para ele mesmo por ser único
                (*queue)->prev = elem;
            }else{
                (*queue)->prev->next = elem;
                elem->next = (*queue);
                elem->prev = (*queue)->prev;

                (*queue)->prev = elem;
            }
        }
    }

}

queue_t *queue_remove (queue_t **queue, queue_t *elem){

    /*if(queue == NULL){
        printf("Fila não existe\n");
    }else{
        if(elem == NULL){
            printf("Elemento não existe\n")
        }else{
            if((*queue) == NULL){
            }
        }
    }*/

}

int queue_size (queue_t *queue){
    int i=0;

    if(queue != NULL){
        queue_t *atual;
        atual = queue;
        do{
            i=i+1;
            atual=atual->next;
        }while (atual != queue);
    }
    printf("%d\n", i);
    return i;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
}
