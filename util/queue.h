//
// DEFINICIÓN DEL TIPO DE DATOS QUE GESTIONA UNA "APPEND-ONLY QUEUE".
// NO PUEDE MODIFICAR ESTE FICHERO.

/*
 * Cola: Lista de entradas gestionada con un esquema FIFO. Solo permite
 * añadir entradas al final. Proporciona acceso por posición a sus
 * elementos.
 *
 * Almacena una referencia (no copia) de cada valor.
 *
 * Al destruir una cola, su implementación invoca la función
 * de liberación especificada como parámetro por cada entrada existente
 * en el mismo para permitir que el usuario de esta colección pueda liberar,
 * si lo considera oportuno, la información asociada a cada clave y valor.
 *
 */

#ifndef _QUEUE_H
#define _QUEUE_H      1

// Tipo opaco para ocultar la implementación
typedef struct queue queue;

// Tipo de datos para una función que accede a una entrada para liberarla.
// Usado por queue_destroy.
typedef void (*func_entry_release_queue_t) (void *value);

// Tipo de datos para una función que accede a una entrada para visitarla.
// Usado por queue_visit.
typedef void (*func_entry_queue_t) (void *value, void *datum);

/* API: COLECCIÓN DE FUNCIONES */

// Crea una cola de solo "append".
// Recibe como parámetro si usa mutex para asegurar exclusión
// mutua en sus operaciones.
// Devuelve una referencia a una cola o NULL en caso de error.
// Si se usa en un entorno multithread, se asume que será utilizada antes
// de crearse los threads que trabajarán con la cola creada.
queue *queue_create(int locking);

// Destruye la cola especificada. Si tiene todavía entradas
// se invocará la función especificada como parámetro por cada una de ellas
// pasando como argumento a la misma el valor asociado a la entrada.
// Si la aplicación no está interesada en ser notificada de las entradas
// existentes, debe especificar NULL en el parámetro de esta función.
// Devuelve 0 si OK y -1 si error.
// Si se usa en un entorno multithread, se asume que será usada después de que
// terminen los threads que han utilizado la cola que se va a destruir.
int queue_destroy(queue *q, func_entry_release_queue_t release_entry);

// Permite iterar sobre todas las entradas de una cola.
// Se invocará la función especificada como parámetro por cada una de ellas
// pasando como argumentos a la misma el valor de la entrada y el dato
// recibido como argumento.
// Devuelve el número de entradas visitadas si OK y -1 si error.
int queue_visit(const queue *q, func_entry_queue_t visit_entry, void *datum);

// Inserta al final de la cola un nuevo elemento.
// Almacena una referencia (y no copia) del valor.
// Devuelve posición en la cola si OK y -1 si error.
int queue_append(queue *q, const void *elem);

// Gets, without extracting, the selected item.
// Since any return value is valid, return in the
// second parameter if an error occurred: 0 if OK and -1 if error.
void * queue_get(const queue *q, int pos, int *error);

// Devuelve el nº de elementos en la cola, -1 si error.
int queue_size(const queue *q);

// Elimina del principio de la cola, que corresponde a las entradas más
// antiguas, el nº de elementos especificados.
// Si se recibe un valor distinto de NULL en el tercer parámetro,
// se invoca esa función como parte de la eliminación de cada entrada.
// Devuelve 0 si OK y -1 si error.

int queue_discard_first_entries(queue *q, int nentries, func_entry_release_queue_t release_entry);

#endif // _QUEUE_H
