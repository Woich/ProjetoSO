// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DAINF UTFPR
// Versão 1.0 -- Março de 2015
//
// interface do driver de disco rígido

#ifndef __DISKDRIVER__
#define __DISKDRIVER__

typedef struct{
    struct pedido *prox;
    struct pedido *ant;

    int numBloco;
    int operacao; //0 é ler | 1 é escrever
    void *bufferPedido;

    struct task_t *tarefaPedido; //Tarefa do pedido
} pedido;

// structura de dados que representa o disco para o SO
typedef struct
{
  int qtdBlocos;
  int tamBlocos;

  struct pedido *filaPedidos;

  struct semaphore_t *semAcesso;

  int acordadoHandler;

  int discoLivre;//0 está livre | 1 está ocupado;

  int statusDormindo;//0 está dormindo | 1 está pronta;
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

#endif
