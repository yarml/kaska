// EJEMPLO DE CLIENTE QUE ENVÍA CON UNA SOLA OPERACIÓN UN ENTERO,
// UN STRING Y UN ARRAY BINARIO. PARA ENVIAR ESTOS DOS ÚLTIMOS, AL
// SER DE TAMAÑO VARIABLE, TRANSMITE ANTES SU TAMAÑO.
// PARA SIMULAR VARIAS PETICIONES, REPITE VARIAS VECES ESA ACCIÓN.
// PUEDE USARLO COMO BASE PARA LA BIBLIOTECA DE CLIENTE.


// EXAMPLE OF A CLIENT THAT SENDS AN INTEGER WITH A SINGLE OPERATION,
// A STRING AND A BINARY ARRAY. TO SEND THESE LAST TWO, TO
// BEING OF VARIABLE SIZE, TRANSMITS ITS SIZE BEFORE.
// TO SIMULATE SEVERAL REQUESTS, REPEAT THAT ACTION SEVERAL TIMES.
// YOU CAN USE IT AS THE BASE FOR THE CLIENT LIBRARY.

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#define NUM_REQ 3 // REPITE VARIAS VECES (COMO SI FUERAN MÚLTIPLES PETICIONES)
		              // REPEAT SEVERAL TIMES (AS IF THERE WERE MULTIPLE REQUESTS)

// inicializa el socket y se conecta al servidor
// initialize the socket and connect to the server
static int init_socket_client(const char *host_server, const char * port) {
    int s;
    struct addrinfo *res;
    // socket stream para Internet: TCP
    // socket stream for internet: TCP
    if ((s=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("error creando socket"); // error creating socket
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /* solo IPv4 */ /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM;

    // obtiene la dirección TCP remota
    // get the remote TCP address
    if (getaddrinfo(host_server, port, &hints, &res)!=0) {
        perror("error en getaddrinfo");
        close(s);
        return -1;
    }
    // realiza la conexión
    // make the connection
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        perror("error en connect");
        close(s);
        return -1;
    }
    freeaddrinfo(res);
    return s;
}

static int peticion(int s, int entero, char *string, int longitud_array,
		void *array) {
        struct iovec iov[5]; // hay que enviar 5 elementos
        int nelem = 0;
        // preparo el envío del entero convertido a formato de red
        int entero_net = htonl(entero);
        iov[nelem].iov_base=&entero_net;
        iov[nelem++].iov_len=sizeof(int);

        // preparo el envío del string mandando antes su longitud
        int longitud_str = strlen(string); // no incluye el carácter nulo
        int longitud_str_net = htonl(longitud_str);
        iov[nelem].iov_base=&longitud_str_net;
        iov[nelem++].iov_len=sizeof(int);
        iov[nelem].iov_base=string; // no se usa & porque ya es un puntero
        iov[nelem++].iov_len=longitud_str;

        // preparo el envío del array mandando antes su longitud
        int longitud_arr_net = htonl(longitud_array);
        iov[nelem].iov_base=&longitud_arr_net;
        iov[nelem++].iov_len=sizeof(int);
        iov[nelem].iov_base=array; // no se usa & porque ya es un puntero
        iov[nelem++].iov_len=longitud_array;

        // modo de operación de los sockets asegura que si no hay error
        // se enviará todo (misma semántica que los "pipes")
        // socket mode of operation ensures that if there is no error
        // everything will be sent (same semantics as "pipes")
        if (writev(s, iov, 5)<0) {
            perror("error en writev");
            close(s);
            return -1;
        }
	int res;
	// recibe un entero como respuesta
	if (recv(s, &res, sizeof(int), MSG_WAITALL) != sizeof(int)) {
            perror("error en recv");
            close(s);
            return -1;
        }
        return ntohl(res);
}

int main(int argc, char *argv[]) {
    int s;
    if (argc!=3) {
        fprintf(stderr, "Uso: %s servidor puerto\n", argv[0]);
        return -1;
    }
    // inicializa el socket y se conecta al servidor
    // initialize the socket and connect to the server
    if ((s=init_socket_client(argv[1], argv[2]))<0) return -1;

    // datos a enviar
    // data to send
    int entero = 12345;
    char *string = "abcdefghijklmnopqrstuvwxyz";
    // podría ser una imagen, un hash, texto cifrado...
    // could be an image, a hash, encrypted text...
    unsigned char array_binario[] = {0x61, 0x62, 0x0, 0x9, 0xa, 0x41, 0x42};
    int res;

    // los envía varias veces como si fueran sucesivas peticiones del cliente
    // que mantiene una conexión persistente
    // send them multiple times as if they were successive client requests
    // which maintains a persistent connection
    for (int i=0; i<NUM_REQ; i++) {
	// cambio algunos valores para la próxima "petición"
  // change some values ​​for the next "request"
	if ((res=peticion(s, entero, string, sizeof(array_binario),
		array_binario))==-1)
	    break;
        printf("Recibida respuesta: %d\n", ntohl(res));

	++entero;
	++array_binario[0];
    }
    close(s); // cierra el socket // close the socket
    return 0;
}
