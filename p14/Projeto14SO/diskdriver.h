// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DAINF UTFPR
// Versão 1.0 -- Março de 2015
//
// interface do driver de disco rígido

#ifndef __DISKDRIVER__
#define __DISKDRIVER__

typedef struct discoPedido{
    struct discoPedido *next;
    struct discoPedido *prev;

    //Tarefa do pedido
    struct task_t *tarefa;

    int bloco;
    //Controla se é ler(0) ou escrever (1)
    int escrever;
    //Buffer do pedido
    void *buffer;

} discoPedido;

// structura de dados que representa o disco para o SO
typedef struct disk_t{
    //Tamanho do disco em blocos
    int tamanhoDisco;
    //Tamanho dos blocos
    int tamanhoBloco;
    //Se está livre
    int isLivre;
    //Se foi chamado por um sinal
    int isSinal;
    //Fila de tarefas esperando
    struct task_t *filaEspera;
    //Fila de pedidos
    struct discoPedido *filaPedidos;
    //Semaforo do disco
    struct semaphore_t semDisco;
    //Posição atual do disco em blocos
    int blocoAtual;

  // preencher com os campos necessarios
} disk_t ;

// inicializacao do driver de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int diskdriver_init (int *numBlocks, int *blockSize) ;

// leitura de um bloco, do disco para o buffer indicado
int disk_block_read (int block, void *buffer) ;

// escrita de um bloco, do buffer indicado para o disco
int disk_block_write (int block, void *buffer) ;

discoPedido* escalonamentoPedidoFCFS();

discoPedido* escalonamentoPedidoSSTF();

discoPedido* escalonamentoPedidoCSCAN();

#endif
