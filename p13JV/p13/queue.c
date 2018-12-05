//------------------------------------------------------------------------------
// DANIELE CLAUDINE MULLER - 1937529
// JOÃO VITOR BABY FONSAKA - 1949772
//------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "queue.h"

//------------------------------------------------------------------------------
// Insere um elemento no final da fila.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - o elemento deve existir
// - o elemento nao deve estar em outra fila

void queue_append (queue_t **queue, queue_t *elem){
    queue_t *itera = *queue;

    if (queue == NULL){
        printf("A fila nao existe!");
        return;
    }
    if (elem == NULL){
        printf("O elemento nao existe!");
        return;
    }
    if (elem->prev != NULL || elem->next != NULL){
        printf("O elemento esta em outra fila!");
        return;
    }
    if (*queue == NULL){
        *queue = elem;
        elem->prev = elem;
        elem->next = elem;
    }
    else {
        while (itera->next != *queue)
            itera = itera->next;
        itera->next = elem;
        elem->prev = itera;
        elem->next = *queue;
        (*queue)->prev = elem;
    }
}

//------------------------------------------------------------------------------
// Remove o elemento indicado da fila, sem o destruir.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - a fila nao deve estar vazia
// - o elemento deve existir
// - o elemento deve pertencer a fila indicada
// Retorno: apontador para o elemento removido, ou NULL se erro

queue_t *queue_remove (queue_t **queue, queue_t *elem){
    queue_t *itera = *queue;
    int achou = 0;

    if (queue == NULL){
        printf("A fila nao existe!");
        return NULL;
    }
    if (*queue == NULL){
        printf("A fila esta vazia!");
        return NULL;
    }
    if (elem == NULL){
        printf("O elemento nao existe!");
        return NULL;
    }

    while (itera->next != *queue){
        if (itera == elem){
            achou = 1;
            break;
        }
        itera = itera->next;
    }
    if (itera == elem)
        achou = 1;

    if (achou == 1){
        if (elem == *queue){
            *queue = elem->next;
            if (elem->next == elem)
                *queue = NULL;
        }

        itera->prev->next = elem->next;
        itera->next->prev = elem->prev;
        elem->prev = NULL;
        elem->next = NULL;

        return elem;
    }

    printf("Elemento nao pertence a fila!");
    return NULL;
}

//------------------------------------------------------------------------------
// Conta o numero de elementos na fila
// Retorno: numero de elementos na fila

int queue_size (queue_t *queue){
    queue_t *itera = queue;
    int cont = 1;

    if (queue == NULL)
        return 0;
    while (itera->next != queue){
        itera = itera->next;
        cont++;
    }
    return cont;
}

//------------------------------------------------------------------------------
// Percorre a fila e imprime na tela seu conteúdo. A impressão de cada
// elemento é feita por uma função externa, definida pelo programa que
// usa a biblioteca. Essa função deve ter o seguinte protótipo:
//
// void print_elem (void *ptr) ; // ptr aponta para o elemento a imprimir

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
    queue_t *itera = queue;

    if (itera == NULL)
        print_elem(NULL);
    else {
        printf("%s", name);
        while(itera->next != queue){
            print_elem(itera);
            itera = itera->next;
        }
        print_elem(itera);
    }
}

