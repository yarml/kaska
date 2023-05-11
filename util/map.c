//
// IMPLEMENTACIÓN DEL TIPO DE DATOS QUE GESTIONA UN MAPA GENÉRICO ITERABLE.
// NO PUEDE MODIFICAR ESTE FICHERO.
// NO ES NECESARIO QUE CONOZCA LOS DETALLES DE LA IMPLEMENTACIÓN PARA USARLO.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "map.h"

// definición de tipos
#define MAGIC_MAP (('M' << 24) + ('A' << 16) + ('P' << 8) + 'A')
#define MAGIC_ITE (('I' << 24) + ('T' << 16) + ('E' << 8) + 'R')
#define MAGIC_POS (('P' << 24) + ('O' << 16) + ('S' << 8) + 'I')

typedef struct entry {
    const void *key;
    void *value;
} entry;

typedef struct map {
    int magic;
    int nentries;
    int length;
    func_cmp_keys_t cmp_keys;
    entry *collection;
    int nholes;
    int hole;
    int locking;
    pthread_mutex_t mut;
    pthread_mutexattr_t mattr;
} map;

typedef struct map_iter {
    int magic;
    map *m;
    int first;
    int count;
    int current;
    map_position *p;
} map_iter;

typedef struct map_position {
    int magic;
    map *m;
    int pos;
} map_position;

// especificación de funciones internas
static int search_entry(const map *m, const void *key);
static void traverse_map(const map *m,  func_entry_map_t func, void *datum);
static int check_map(const map *m);
static int check_position(map *m, map_position *p);
static int check_iter(map_iter *i);
    
// funciones de compararación predefinidas para varios tipos de claves

// para strings
int key_string(const void *k1, const void *k2) {
    return !strcmp(k1, k2);
}

// para enteros
int key_int(const void *k1, const void *k2) {
    return *(int *)k1 == *(int *)k2;
}

/* implementación de funciones externas (API) */

map *map_create(func_cmp_keys_t cmp, int locking) {
    map *m = malloc(sizeof(map));
    if (!m) return NULL;
    m->magic=MAGIC_MAP;
    m->nentries=m->nholes=m->length=0;
    m->hole=-1;
    m->cmp_keys=cmp;
    m->collection=NULL;
    m->locking=locking;
    if (locking) {
        pthread_mutexattr_init(&m->mattr);
        pthread_mutexattr_settype(&m->mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m->mut, &m->mattr);
    }
    return m;
}
// destruye un mapa
int map_destroy(map *m, func_entry_release_map_t release_entry){
    if (check_map(m)==-1) return -1;
    if (release_entry)
        for (int i=0; i<m->length; i++)
	    if (m->collection[i].key!=(void *)-1)
                release_entry((void *)m->collection[i].key, (void *)m->collection[i].value);
    free(m->collection);
    m->magic=0;
    if (m->locking) {
        pthread_mutex_destroy(&m->mut);
        pthread_mutexattr_destroy(&m->mattr);
    }
    free(m);
    return 0;
}
// itera sobre todas las entradas del mapa
int map_visit(const map *m, func_entry_map_t visit_entry, void *datum){
    int err = 0;
    if (check_map(m)==-1) return -1;
    if (visit_entry) {
        if (m->locking) pthread_mutex_lock((pthread_mutex_t *)&m->mut);
	    traverse_map(m, visit_entry, datum);
        if (m->locking) pthread_mutex_unlock((pthread_mutex_t *)&m->mut);
    }
    return err;
}
// inserta entrada en el mapa
int map_put(map *m, const void *key, void *value) {
    if (check_map(m)==-1 || !key)  return -1;
    int res=0;
    if (m->locking) pthread_mutex_lock(&m->mut);
    if (search_entry(m, key)!=-1)  res = -1;
    else {
        entry e = {key, value};
	if (m->nholes==0) {
            m->length++;
       	    m->collection=realloc(m->collection, m->length*sizeof(entry));
            if (m->collection) {
                m->collection[m->length-1] = e;
	    }
	    else res=-1;
	}
	else {
	    if (m->hole!=-1) {
                m->collection[m->hole] = e;
		m->hole=-1;
	    }
	    else {
	        for (int i=0; i<m->length; i++) {
		    if (m->collection[i].key == (void *)-1) {
                        m->collection[i] = e;
			break;
                    }
                }
            }
	    m->nholes--;
        }
    }
    if (res==0) m->nentries++;
    if (m->locking) pthread_mutex_unlock(&m->mut);
    return res;
}
// retorna valor asociado a la clave
void * map_get(const map *m, const void *key, int *error){
    int ne, err=0;
    void *v;
    if (check_map(m)==-1 || !key)  err= -1;
    else {
        if (m->locking) pthread_mutex_lock((pthread_mutex_t *)&m->mut);
        if ((ne=search_entry(m, key))==-1)  err = -1;
        else v = (void *)m->collection[ne].value;
        if (m->locking) pthread_mutex_unlock((pthread_mutex_t *)&m->mut);
    }
    if (error) *error=err;
    return (err==-1?NULL:v);
}
// elimina una entrada dada su clave
int map_remove_entry(map *m, const void *key, func_entry_release_map_t release_entry) {
    int ne, res=0;
    if (check_map(m)==-1 || !key)  return -1;
    if (m->locking) pthread_mutex_lock(&m->mut);
    if ((ne=search_entry(m, key))==-1)  res = -1;
    else {
        if (release_entry)
            release_entry((void *)m->collection[ne].key,
                (void *)m->collection[ne].value);
        m->nentries--;
        m->collection[ne].key = (void *)-1; // lo marca como hueco
        m->nholes++;
        m->hole=ne;
    }
    if (m->locking) pthread_mutex_unlock(&m->mut);
    return res;
}
// Devuelve cuántas entradas tiene un mapa.
int map_size(const map *m) {
    if (check_map(m)==-1)  return -1;
    return m->nentries;
}
map_position * map_alloc_position(map *m) {
    map_position *p = malloc(sizeof(map_position));
    if (p) {
        p->magic = MAGIC_POS;
        p->m = m;
        p->pos = -1;
    }
    return p;
}
int map_free_position(map_position *p) {
    if (p && check_position(p->m, p)==-1) return -1;
    if (p) {
        p->magic = 0;
        free(p);
    }
    return 0;
}
// crea un iterador asociado al mapa
map_iter *map_iter_init(map *m, map_position *p) {
    if (check_map(m)==-1) return NULL;
    if (check_position(m, p)==-1) return NULL;
    if (m->locking) pthread_mutex_lock(&m->mut);
    if ((p->pos==-1) && m->nentries)
        for (p->pos=0; m->collection[p->pos].key==(void *)-1; p->pos = (p->pos+1)%m->length);
    map_iter *it = malloc(sizeof(map_iter));
    it->magic=MAGIC_ITE;
    it->m=m;
    it->p=p;
    it->first=it->current=p->pos;
    it->count=m->nentries;
    return it;
}
// comprueba si el iterador no ha llegado al final
int map_iter_has_next(map_iter *it) {
    return (check_iter(it)!=-1 && (it->count>0));
}
// avanza el iterador
int map_iter_next(map_iter *it){
    if (check_iter(it)==-1 || (it->count <= 0)) return -1;
    if (it->m->locking) pthread_mutex_lock(&it->m->mut);
    int pos = (it->current+1)%it->m->length;
    for (; it->m->collection[pos].key==(void *)-1; pos = (pos+1)%it->m->length);
    it->current = pos;
    it->count--;
    if (it->m->locking) pthread_mutex_unlock(&it->m->mut);
    return 0;
}
// obtiene el elemento apuntado por el iterador, devolviendo la
// clave y el valor asociado al mismo.
int map_iter_value(map_iter *it, const void **key, void **value) {
    if (check_iter(it)==-1 || (it->count <= 0)) return -1;
    if (it->m->locking) pthread_mutex_lock(&it->m->mut);
    if (key) *key = it->m->collection[it->current].key;
    if (value) *value = it->m->collection[it->current].value;
    if (it->m->locking) pthread_mutex_unlock(&it->m->mut);
    return 0;
}
// destruye un iterador devolviendo en qué posición se ha quedado
map_position * map_iter_exit(map_iter *it) {
    if (check_iter(it)==-1) return NULL;
    if (it->m->locking) pthread_mutex_lock((pthread_mutex_t *)&it->m->mut);
    map_position *p = it->p;
    if (check_position(it->m, p)==-1) return NULL;
    p->pos = it->current;
    it->magic=0;
    if (it->m->locking) pthread_mutex_unlock(&it->m->mut);
    free(it);
    return p;
}

// implementación de funciones internas
static int search_entry(const map *m, const void *key){
    int i;
    if (!key) return -1;
    for (i=0; (i<m->length && (m->collection[i].key==(void *)-1 || !m->cmp_keys(key, m->collection[i].key))); i++);
    return (i==m->length?-1:i);
}
static void traverse_map(const map *m, func_entry_map_t func, void *datum){
    int count = m->nentries;
    for (int i=0; count; i++)
	if (m->collection[i].key!=(void *)-1) {
            func((void *)m->collection[i].key, (void *)m->collection[i].value, datum);
	    count--;
	}
}
static int check_map(const map *m){
    int res=0;
    if (m==NULL || m->magic!=MAGIC_MAP){
        res=-1; fprintf(stderr, "el mapa especificado no es válido\n");
    }
    return res;
}
static int check_position(map *m, map_position *p){
    int res=0;
    if (!p || p->magic!=MAGIC_POS || p->m!=m || ((p->pos != -1) && ((p->pos < 0) || (p->pos>=p->m->length)))) {
        res=-1; fprintf(stderr, "la posición especificada no es válida\n");
    }
    return res;
}
static int check_iter(map_iter *i){
    int res=0;
    if (!i || i->magic!=MAGIC_ITE || check_map(i->m)==-1 || (i->current < 0) || (i->current>=i->m->length) ) { 
        res=-1; fprintf(stderr, "el iterador especificado no es válido\n");
    }
    return res;
}
