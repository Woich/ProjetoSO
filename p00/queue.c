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
    /*Váriavel controlar se elemento buscado existe na fila enviada*/
    int existeFila=0;

    if(queue == NULL){//checa se a fila existe
        printf("Fila não existe\n");
    }else{
        if(elem == NULL){//checa se o elemento buscado existe
            printf("Elemento não existe\n");
        }else{
            if((*queue) == NULL){//checa se a lista possui algum elmento dentro dela
                printf("Lista está vázia\n");
            }else{
                queue_t *atual;//Define o elemento a ser avaliado no momento inicializando ele como o primeiro elemento
                atual = queue;
        /*Loop para buscar na lista desejada*/
                while((atual->next != queue)){
                /* Valida se o elemento buscado foi encontrado */
                    if(atual == elem){
                        if(atual->next == atual){//Valida se a lista possui apena um elmento, caso sim apaga todas as refrencias dele sem destruí-lo
                            atual->next = NULL;
                            atual->prev = NULL;
                            (*queue) =  NULL;
                        }else{
                        /*Nesse bloco ele conecta o anterior e o próximo do elemento avaliado de forma a iginorar o que está sendo removido*/
                            atual->next->prev = atual->prev;
                            atual->prev->next = atual->next;

                            if(*queue == elem){//caso o que será retirado seja o primeiro da lista
                                (*queue) = (*queue)->next;
                            }
                        /*apaga todas as refrencias dele sem destruí-lo*/
                            atual->next = NULL;
                            atual->prev = NULL;
                        }

                        existeFila = 1;
                        return(atual);
                    }

                    atual = atual->next;
                }

                if(existeFila == 0){
                    printf("Elemento selecionado não existe na fila eviada\n");
                }

                return(NULL);

            }
        }
    }

}

int queue_size (queue_t *queue){
    int i=0;//inicializa o contador

    if(queue != NULL){//avalia se a lista existe
        queue_t *atual;
        atual = queue;//declara a váriavel e atribui ela como o primeiro elemento da lista
        do{//precisa ser um do para que primeiro seja realizados as operações e depois feito a comparação para funcionar. Esse conclusão foi retirada de testes.
            i=i+1;
            atual=atual->next;
        }while (atual != queue);
    }
    return i;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
}
