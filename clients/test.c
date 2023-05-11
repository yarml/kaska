#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "kaska.h"

static char * leer_string(char *prompt) {
    int n;
    char *str;
    char *lin=NULL;
    size_t ll=0;
    fputs(prompt, stdout);
    n=getline(&lin, &ll, stdin);
    if (n<1) {ungetc(' ', stdin); return NULL;}
    n=sscanf(lin, "%ms", &str);
    free(lin);
    if (n!=1) return NULL;
    return str;
}
int leer_int(char *prompt) {
    int n, v;
    char *lin=NULL;
    size_t ll=0;
    fputs(prompt, stdout);
    n=getline(&lin, &ll, stdin);
    if (n<1) {ungetc(' ', stdin); return -1;}
    n=sscanf(lin, "%d", &v);
    free(lin);
    if (n!=1) return -1;
    return v;
}
static char * leer_msg(int *size) {
    int sz;
    int n;
    void *msg;
    size_t ll=0;
    char *lin=NULL;
    printf("Introduzca la longitud del mensaje: ");
    n=getline(&lin, &ll, stdin);
    if (n<1) {ungetc(' ', stdin); return NULL;}
    n=sscanf(lin, "%d", &sz);
    free(lin);
    if (n!=1) return NULL;
    msg = malloc(sz);
    printf("Introduzca el mensaje: ");
    n=fread(msg, 1, sz, stdin);
    if (n!=sz) return NULL;
    ll=0; lin=NULL;
    n=getline(&lin, &ll, stdin);
    *size=sz;
    return msg;
}
static int leer_temas(char ***t) {
    int nt;
    char *tema;
    nt=leer_int("Introduzca el número de temas: ");
    if (nt==-1) return -1;
    *t=malloc(nt*sizeof(char *));
    for (int i=0; i<nt; i++) {
	tema=leer_string("Introduzca el nombre del tema: ");
	if (!tema) {
            for (int j=0; j<i; j++) free((*t)[j]);
	    free(*t);
	    return -1;
	}
	(*t)[i]=tema;
    }
    return nt;
}
int main(int argc, char *argv[]) {
    char *op;
    char *tema, ** temas;
    int ntemas, off, res;
    int msg_sz;
    void *msg;

    while (1) {
	op=leer_string("\nSeleccione operación (ctrl-D para terminar; en menús internos ctrl-D para volver a menú principal)\n\tC: Create topic|N: Ntopics|M: send_Msg|L: message Length|E: End offset\n\tS: Subscribe|U: Unsubscribe|T: posiTion|K: seeK|P: Poll\n\tO: cOmmit|D: committeD|Y: commit=position|Z: seek=commited\n");
        if (op==NULL) break;
        switch(op[0]) {
            case 'C':
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
                if (create_topic(tema)<0)
                    printf("error en create_topic\n");
                else
                    printf("se ha creado el tema %s\n", tema);
                break;
            case 'N':
                if ((res=ntopics())<0)
                    printf("error en ntopics\n");
                else
                    printf("en el sistema hay %d temas\n", res);
                break;
            case 'M':
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
		msg=leer_msg(&msg_sz);
                if ((msg==NULL) || (msg_sz==0)) {free(tema); continue;}
                if ((res=send_msg(tema, msg_sz, msg))<0)
                    printf("error en send_msg\n");
                else {
                    printf("se ha enviando mensaje de tamaño %d al tema %s que queda ubicado en offset %d\n\tmensaje: ", msg_sz, tema, res);
		    fflush(stdout);
		    write(1, msg, msg_sz);
                    printf(" (hexadecimal: ");
                    for (int i=0; i<msg_sz; i++) printf("%02x", ((char *)msg)[i]);
                    printf(")\n");
                }
		free(tema); free(msg);
                break;
            case 'L':
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
		off=leer_int("Introduzca el offset: ");
                if (off==-1) continue;
                if ((res=msg_length(tema, off))<0)
                    printf("error en msg_length\n");
                else
                    printf("la longitud del mensaje %d del tema %s es %d\n",
		        off, tema, res);
		free(tema);
                break;
            case 'E':
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
                if ((res=end_offset(tema))<0)
                    printf("error en end_offset\n");
                else
                    printf("el offset/longitud de la cola del tema %s es %d\n",
		        tema, res);
		free(tema);
                break;
            case 'S':
                ntemas=leer_temas(&temas);
                if (ntemas==-1) continue;
                if ((res=subscribe(ntemas, temas))<0)
                    printf("error en suscripción de temas\n");
                else
                    printf("se ha suscrito a %d temas\n", res);
                break;
            case 'U':
                if (unsubscribe()<0)
                    printf("error en unsubscribe\n");
                else
                    printf("unsubscribe OK\n");
                break;
            case 'T':
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
                if ((off=position(tema))<0)
                    printf("error en position\n");
                else
                    printf("offset en el cliente para el tema %s es %d\n",
			tema, off);
		free(tema);
                break;
            case 'K':
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
		off=leer_int("Introduzca el offset: ");
                if (off==-1) continue;
                if (seek(tema, off)<0)
                    printf("error en seek\n");
                else
                    printf("nuevo offset %d en el cliente para el tema %s\n",
			off, tema);
		free(tema);
                break;
            case 'P':
                if ((msg_sz=poll(&tema, &msg))<0)
                    printf("error en poll\n");
                else if (msg_sz==0)
		    printf("no hay mensajes\n");
		else {
                    printf("recibido mensaje de tamaño %d del tema %s\n\tmensaje: ", msg_sz, tema);
		    fflush(stdout);
		    write(1, msg, msg_sz);
                    printf(" (hexadecimal: ");
                    for (int i=0; i<msg_sz; i++) printf("%02x", ((char *)msg)[i]);
                    printf(")\n");
		    if (msg) free(msg);
		    if (tema) free(tema);
		}
                break;
            case 'O':
		if (argc == 1) {
		    printf("Para realizar esta operación debe ejecutar de nuevo este programa pasándole como argumento el nombre del cliente\n");
		    break;
		}
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
		off=leer_int("Introduzca el offset: ");
                if (off==-1) continue;
                if (commit(argv[1], tema, off)<0)
                    printf("error en commit\n");
                else
                    printf("cliente %s guarda offset %d para el tema %s\n",
			argv[1], off, tema);
		free(tema);
                break;
            case 'D':
		if (argc == 1) {
		    printf("Para realizar esta operación debe ejecutar de nuevo este programa pasándole como argumento el nombre del cliente\n");
		    break;
		}
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
                if ((off=commited(argv[1], tema))<0)
                    printf("error en commited\n");
                else
                    printf("cliente %s obtiene offset guardado %d para el tema %s\n",
			argv[1], off, tema);
		free(tema);
                break;
            case 'Y':
		if (argc == 1) {
		    printf("Para realizar esta operación debe ejecutar de nuevo este programa pasándole como argumento el nombre del cliente\n");
		    break;
		}
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
                if ((off=position(tema))<0)
                    printf("error en position\n");
                else {
		    if (commit(argv[1], tema, off)<0)
                        printf("error en commit\n");
                    else 
                        printf("offset commit = position (%d) para el tema %s\n", off, tema);
		}
		free(tema);
                break;
            case 'Z':
		if (argc == 1) {
		    printf("Para realizar esta operación debe ejecutar de nuevo este programa pasándole como argumento el nombre del cliente\n");
		    break;
		}
		tema=leer_string("Introduzca el nombre del tema: ");
                if (tema==NULL) continue;
                if ((off=commited(argv[1], tema))<0)
                    printf("error en commited\n");
                else {
		    if (seek(tema, off)<0)
                        printf("error en seek\n");
                    else 
                        printf("offset cliente = commited (%d) para el tema %s\n", off, tema);
		}
		free(tema);
                break;
            default:
                    printf("operación no válida\n");
            }
        if (op) free(op);
    };
    return 0;
}

