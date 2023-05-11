// EJEMPLO DE USO DE LOS TIPOS DE DATOS "MAP" Y "QUEUE"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "map.h"
#include "queue.h"

typedef struct cuenta {
    char *titular;
    int edad;
    int saldo;
    queue *operaciones;
} cuenta;

typedef struct operacion {
    int cantidad;
    time_t fecha;
} operacion;

static void imprime_operacion(operacion *o) {
    printf("cantidad %d fecha %s", o->cantidad, asctime(localtime(&o->fecha)));
}
static void imprime_entrada_cola(void *v, void *d) {
    operacion *o = v;
    printf("\t");
    imprime_operacion(o);
}
static void imprime_cuenta(cuenta *c) {
    printf("titular %s edad %d saldo %d\n", c->titular,
	c->edad, c->saldo);
}
static void imprime_entrada_mapa(void *k, void *v, void *d) {
    // se puede acceder al titular tanto usando la clave como a través del valor
    cuenta *c = v;
    printf("%s: ", (char *)d);
    imprime_cuenta(c);
    // recorre todos los elementos de la cola
    queue_visit(c->operaciones, imprime_entrada_cola, NULL);
}

static void libera_operacion(void *v) {
    free(v);
}
static void libera_cuenta(void *k, void *v) {
    // la clave es un string literal: no hay que liberarla
    // pero el valor sí;
    // antes hay que eliminar la cola
    cuenta *c = v;
    queue_destroy(c->operaciones, libera_operacion);
    if (v) free(v);
}

int main(int argc, char *argv[]) {
    map *cuentas;
    int err = 0;
    cuenta *c;
    operacion *op;
    map_position *pos;
    map_iter *it;
    bool encontrado;

    // let's use an example with some similarity to practice
    // where a map is used such that on the value associated with an input
    // of the same, which has a string as key, there is a queue.

    // this is a collection of bank accounts, such that each account
    // has associated the name of the holder, his age, the balance and a
    // queue of operations, including each operation the amount and date;
    // to keep all the information for a pooled account,
    // it has been decided to also include the key (holder) in the structure
    // which is used as a value

    // create the map of accounts: since we are not going to use it from multiple
    // threads, 2nd parameter = 0 (does not use mutex).
    // IN PRACTICE WE WILL CREATE A MAP AT THE INITIALIZATION OF THE BROKER
    // AND ANOTHER IN THE CLIENT LIBRARY TO STORE THE SUBSCRIPTIONS.
    // ON BOTH MAPS THE NAME OF THE THEME WILL ACT AS THE KEY. NOTE THAT THE
    // FIRST MAP WILL BE ACCESSED FROM MULTIPLE THREADS
    cuentas = map_create(key_string, 0);

    // vamos a crear varias cuentas;
    // EN EL BROKER HAY QUE AÑADIR UNA NUEVA ENTRADA POR CADA TEMA CREADO
    // ASOCIANDO UNA COLA VACÍA CON EL TEMA.
    // EN LA BIBLIOTECA DE CLIENTE SE AÑADE UNA NUEVA ENTRADA POR CADA TEMA
    // SUSCRITO NO REQUIRIÉNDOSE UNA COLA.
    cuenta *c1 = malloc(sizeof(cuenta));
    c1->titular="Juan"; c1->edad=11; c1->saldo=1000;
    c1->operaciones=queue_create(0); // sin cerrojos
    map_put(cuentas, c1->titular, c1);

    cuenta *c2 = malloc(sizeof(cuenta));
    c2->titular="Luis"; c2->edad=22; c2->saldo=2000;
    c2->operaciones=queue_create(0);
    map_put(cuentas, c2->titular, c2);

    cuenta *c3 = malloc(sizeof(cuenta));
    c3->titular="Sara"; c3->edad=33; c3->saldo=3000;
    c3->operaciones=queue_create(0);
    map_put(cuentas, c3->titular, c3);

    cuenta *c4 = malloc(sizeof(cuenta));
    c4->titular="Ana"; c4->edad=44; c4->saldo=4000;
    c4->operaciones=queue_create(0);
    map_put(cuentas, c4->titular, c4);

    // ya existe, da error
    // EN LA PRÁCTICA NOS PERMITIRÁ DETECTAR QUE UN TEMA YA EXISTE EN EL
    // BROKER O QUE EL CLIENTE YA ESTÁ SUSCRITO A ESE TEMA.
    if (map_put(cuentas, c1->titular, c1)<0)
	printf("map_put con clave ya existente: debe detectarse error\n");

    // imprimimos el tamaño del mapa
    // EN LA PRÁCTICA SE NECESITA EL TAMAÑO DEL MAPA, POR EJEMPLO, EN
    // LA OPERACIÓN NTOPICS.
    printf("\nHay %d cuentas\n\n", map_size(cuentas));
    
    // vamos a acceder a alguna entrada del mapa e imprimir algún dato;
    // EN LA PRÁCTICA, ESTA ACCIÓN SE REQUIERE EN CASI TODAS LAS OPERACIONES
    // TANTO DEL BROKER COMO DE LA BIBLIOTECA DE CLIENTE.
    c = map_get(cuentas, "Sara", &err);
    if (err == -1)
	printf("no debe salir\n");
    printf("Saldo de la cuenta de Sara %d\n\n", c->saldo);

    // vamos a acceder a alguna entrada del mapa para modificarla;
    // EN LA PRÁCTICA, POR EJEMPLO, PARA MODIFICAR EL OFFSET DE UN TEMA
    // EN LA BIBLIOTECA DE CLIENTE.
    c = map_get(cuentas, "Luis", &err);
    if (err == -1)
	printf("no debe salir\n");
    ++c->edad; // cumpleaños de Luis

    // para depurar imprimimos los valores recorriendo todo el mapa;
    // en la función "imprime_entrada_mapa" se incluye a su vez un
    // "queue_visit" para imprimir el contenido de la cola (por ahora, vacía).
    // El último parámetro será recibido por la función especificada.
    // EN LA PRÁCTICA NOS VA A FACILITAR LA DEPURACIÓN AL PODER MOSTRAR
    // EL ESTADO DE LOS MAPAS Y, EN EL CASO DEL BROKER, DE LAS COLAS ASOCIADAS.
    map_visit(cuentas, imprime_entrada_mapa, "Estado después del cumpleaños de Luis");

    // vamos a añadir varias operaciones a la cuenta de Juan
    // EN LA PRÁCTICA, SE REQUIERE EN EL TRATAMIENTO DE LA OPERACIÓN SEND_MSG
    // EN EL BROKER.
    c = map_get(cuentas, "Juan", &err);
    operacion *op1 = malloc(sizeof(operacion));
    op1->cantidad=100; op1->fecha=time(NULL);
    queue_append(c->operaciones, op1);
    sleep(1);
    operacion *op2 = malloc(sizeof(operacion));
    op2->cantidad=200; op2->fecha=time(NULL);
    queue_append(c->operaciones, op2);
    sleep(1);
    operacion *op3 = malloc(sizeof(operacion));
    op3->cantidad=-150; op3->fecha=time(NULL);
    queue_append(c->operaciones, op3);
    
    // imprimimos el tamaño de la cola de operaciones asociada a Juan
    // EN LA PRÁCTICA, SE USA, POR EJEMPLO, EN EL TRATAMIENTO DE LA
    // OPERACIÓN END_OFFSET EN EL BROKER.
    printf("\nEn la cuenta de %s hay %d operaciones\n\n",
		 c->titular, queue_size(c->operaciones));

    // para depurar comprobamos de nuevo los valores;
    // recuerde que se imprimen también las operaciones porque hemos incluido un
    // queue_visit dentro de imprime_entrada_mapa.
    // EN LA PRÁCTICA, INCLÚYALO EN TODOS LOS PUNTOS SIGNIFICATIVOS.
    map_visit(cuentas, imprime_entrada_mapa, "Estado después de añadir ops a Juan");
    
    // vamos a acceder a la segunda operación (pos=1) de la cuenta de Juan;
    // EN LA PRÁCTICA SE USARÁ EN EL BROKER TANTO PARA MSG_LENGTH COMO POLL 

    op = queue_get(c->operaciones, 1, &err);
    if (err!=-1) {
        printf("\nSegunda operación de la cuenta de %s: ", c->titular);
	imprime_operacion(op);
    }

    // ahora vamos a realizar varias iteraciones sobre el mapa;
    // cada iteración debe comenzar justo después de donde terminó
    // la previa.
    // EN LA PRÁCTICA SE USARÁ EN LA FUNCIÓN POLL DE LA BIBLIOTECA CLIENTE

    // en primer lugar, hay que habilitar una variable para guardar
    // la posición donde comienzan y terminan las iteraciones.
    // EN LA PRÁCTICA SE USARÁ EN LA FUNCIÓN SUBSCRIBE DE LA BIBLIOTECA CLIENTE
    pos = map_alloc_position(cuentas);

    // primera iteración: hasta que se encuentra titular con edad > 20
    // EN LA PRÁCTICA, EN LA BIBLIOTECA DE CLIENTE, POLL ENCUENTRA UN MENSAJE
    it = map_iter_init(cuentas, pos);
    for (encontrado=false; it && !encontrado && map_iter_has_next(it); map_iter_next(it)) {
	map_iter_value(it, NULL, (void **) &c); // interesados solo en el valor
	encontrado = c->edad>20;
    }
    if (encontrado) {
        printf("\nUna cuenta cuyo titular tiene más de 20 años\n\t");
	imprime_cuenta(c);
    }

    // liberamos el iterador previo guardando su posición que será justo la
    // siguiente a la encontrada ya que se ha ejecutado después "map_iter_next"
    pos = map_iter_exit(it);

    // segunda iteración: recorre todas las cuentas empezando justo
    // después de donde se quedó la iteración previa
    // EN LA PRÁCTICA, EN LA BIBLIOTECA DE CLIENTE, POLL NO ENCUENTRA UN MENSAJE
	    
    printf("\nTodas las cuentas empezando por donde terminó iteración previa\n");
    for (it = map_iter_init(cuentas, pos); it && map_iter_has_next(it); map_iter_next(it)) {
	map_iter_value(it, NULL, (void **) &c); // interesados solo en el valor
	imprime_cuenta(c);
    }
    // iterador completado: próxima iteración comenzará desde el mismo punto
    pos = map_iter_exit(it);
    
    // tercera iteración: recorre todas las cuentas empezando desde el mismo punto
    // EN LA PRÁCTICA, EN LA BIBLIOTECA DE CLIENTE, DESPUÉS DE POLL QUE
    // NO ENCUENTRA UN MENSAJE
    printf("\nNuevamente todas las cuentas empezando desde el mismo punto\n");
    for (it = map_iter_init(cuentas, pos); it && map_iter_has_next(it); map_iter_next(it)) {
	map_iter_value(it, NULL, (void **) &c); // interesados solo en el valor
	imprime_cuenta(c);
    }
    pos = map_iter_exit(it);

    // liberamos todas las estructuras de datos;
    // en libera_cuenta hemos incluido la destrucción de las cola
    // EN LA PRÁCTICA SE USARÁ EN UNSUBSCRIBE
    map_free_position(pos);
    map_destroy(cuentas, libera_cuenta);
    return 0;
}
