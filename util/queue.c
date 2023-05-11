//
// IMPLEMENTACIÓN DEL TIPO DE DATOS QUE GESTIONA UNA "APPEND-ONLY QUEUE".
//
// NO PUEDE MODIFICAR ESTE FICHERO.
// NO ES NECESARIO QUE CONOZCA LOS DETALLES DE LA IMPLEMENTACIÓN PARA USARLO.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"

// definición de tipos
struct entry {
    const void *elem;
};
#define MAGIC_QUE (('C' << 24) + ('O' << 16) + ('L' << 8) + 'A')

struct queue {
    int magic;
    int nentries;
    int length;
    struct entry *collection;
    int locking;
    pthread_mutex_t mut;
    pthread_mutexattr_t mattr;
};

// especificación de funciones internas
static void traverse_queue(const queue *q,  func_entry_queue_t func, void *datum);
static int check_queue(const queue *q);
    
/* implementación de funciones externas (API) */

// crea una cola
queue *queue_create(int locking) {
    queue *q = malloc(sizeof(queue));
    if (!q) return NULL;
    q->magic=MAGIC_QUE;
    q->nentries=q->length=0;
    q->collection=NULL;
    q->locking=locking;
    if (locking) {
        pthread_mutexattr_init(&q->mattr);
        pthread_mutexattr_settype(&q->mattr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&q->mut, &q->mattr);
    }
    return q;
}
// elimina una cola
int queue_destroy(queue *q, func_entry_release_queue_t release_entry){
    if (check_queue(q)==-1) return -1;
    if (release_entry)
        for (int i=0; i<q->nentries; i++)
                release_entry((void *)q->collection[i].elem);
    free(q->collection);
    q->magic=0;
    if (q->locking) {
        pthread_mutex_destroy(&q->mut);
        pthread_mutexattr_destroy(&q->mattr);
    }
    free(q);
    return 0;
}
// itera sobre una cola
int queue_visit(const queue *q, func_entry_queue_t visit_entry, void *datum) {
    if (check_queue(q)==-1) return -1;
    int err = 0;
    if (visit_entry) {
        if (q->locking) pthread_mutex_lock((pthread_mutex_t *)&q->mut);
            traverse_queue(q, visit_entry, datum);
        if (q->locking) pthread_mutex_unlock((pthread_mutex_t *)&q->mut);
    }
    return err;
}
// accede a un elemento de la cola
void * queue_get(const queue *q, int pos, int *error){
    int err=0;
    void *v;
    if (check_queue(q)==-1) err=-1;
    else {
        if (q->locking) pthread_mutex_lock((pthread_mutex_t *)&q->mut);
        if (pos < 0 || pos >= q->nentries)
            err = -1;
        else v = (void *)q->collection[pos].elem;
        if (q->locking) pthread_mutex_unlock((pthread_mutex_t *)&q->mut);
    }
    if (error) *error=err;
    return (err==-1?NULL:v);
}

// Inserta al final de la cola un nuevo elemento.
int queue_append(queue *q, const void *elem) {
    if (check_queue(q) || !elem ) return -1;
    int res=0;
    if (q->locking) pthread_mutex_lock(&q->mut);
    q->nentries++;
    if (q->nentries>q->length) {
        q->collection=realloc(q->collection, q->nentries*sizeof(struct entry));
        if (!q->collection)
	    res=-1;
	else 
            q->length++;
    }
    if (res!=-1) {
            struct entry e = {elem};
            q->collection[q->nentries-1] =  e;
	    res=q->nentries-1;
    }
    if (q->locking) pthread_mutex_unlock(&q->mut);
    return res;
}

// Elimina del principio de la cola el nº de elementos especificados.
int queue_discard_first_entries(queue *q, int nentries, func_entry_release_queue_t release_entry) {
    if (check_queue(q) || nentries < 0 || nentries > q->nentries) return -1;
    int res=0;
    if (q->locking) pthread_mutex_lock(&q->mut);
    if (release_entry)
        for (int i=0; i<nentries; i++)
                release_entry((void *)q->collection[i].elem);
    q->nentries-=nentries;
    memmove(&q->collection[0], &q->collection[nentries], q->nentries);
    if (q->locking) pthread_mutex_unlock(&q->mut);
    return res;
}
int queue_size(const queue *q){
    if (check_queue(q)) return -1;
    return q->nentries;
}

// implementación de funciones internas
static void traverse_queue(const queue *q,  func_entry_queue_t func, void *datum) {
    for (int i=0; i<q->nentries; i++)
        func((void *)q->collection[i].elem, datum);
}
static int check_queue(const queue *q){
    int res=0;
    if (q==NULL || q->magic!=MAGIC_QUE){
        res=-1; fprintf(stderr, "la cola especificada no es válida\n");
    }
    return res;
}
