#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "comun.h"
#include "kaska.h"
#include "map.h"

// This function is called first by all library functions to make sure
// that a connection is established. It returns a socket descriptor
// that can be used to send data to broker.
static int ensure_connected()
{
  static int init = 0; // Initial value is 0
                       // If non zero, the connection was already established
  static int sfd;
  int status;

  if (init)
    return sfd;

  init = 1;

  char *port = getenv("BROKER_PORT");
  char *hostname = getenv("BROKER_HOST");

  if (!port || !hostname)
    return -1;

  // Connection not established yet
  sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sfd < 0)
  {
    perror("socket");
    return sfd;
  }

  // Obtain the host address
  struct addrinfo *res;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; /* solo IPv4 */ /* IPv4 only */
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(hostname, port, &hints, &res) != 0)
  {
    perror("getaddrinfo");
    close(sfd);
    return -1;
  }

  // Attempt to connect to broker
  status = connect(sfd, res->ai_addr, res->ai_addrlen);
  if (status < 0)
  {
    perror("connect");
    close(sfd);
    freeaddrinfo(res);
    return -1;
  }

  freeaddrinfo(res);

  return sfd;
}

static void iove_setup(struct iovec *iov, size_t index, size_t len, void *base)
{
  iov[index].iov_base = base;
  iov[index].iov_len = len;
}

// Crea el tema especificado.
// Devuelve 0 si OK y un valor negativo en caso de error.
int create_topic(char *topic)
{
  size_t topic_len = strlen(topic);
  if (topic_len >= 216)
    return -1;
  int sfd = ensure_connected();
  if (sfd < 0)
    return -1;
  /* CREATE_TOPIC has the following format */
  //  1 byte: Opcode
  //  4 bytes: topic name length (network order) = N
  //  N bytes: Null terminated topic name
  struct iovec iov[3];

  uint8_t op = OP_CREATE_TOPIC;
  uint32_t topic_len_net = htonl(topic_len + 1);

  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, topic_len + 1, topic);

  if(writev(sfd, iov, 3) < 0)
    return -1;
  // Receive response
  uint8_t result;
  if(recv(sfd, &result, sizeof(result), MSG_WAITALL) <= 0)
    return -1;
  return -result;
}
// Devuelve cuántos temas existen en el sistema y un valor negativo
// en caso de error.
int ntopics(void)
{
  int sfd = ensure_connected();
  if (sfd < 0)
    return -1;
  /* NTOPICS has the following format */
  //  1 byte: Opcode
  uint8_t op = OP_NTOPICS;
  if(write(sfd, &op, 1) < 0)
    return -1;

  // Receive response
  // 4 bytes: Number of topics (network order)
  uint32_t ntopics_net;
  if(recv(sfd, &ntopics_net, 4, MSG_WAITALL) <= 0)
    return -1;
  uint32_t ntopics = ntohl(ntopics_net);
  return ntopics;
}

// SEGUNDA FASE: PRODUCIR/PUBLICAR

// Envía el mensaje al tema especificado; nótese la necesidad
// de indicar el tamaño ya que puede tener un contenido de tipo binario.
// Devuelve el offset si OK y un valor negativo en caso de error.
int send_msg(char *topic, int msg_size, void *msg)
{
  int sfd = ensure_connected();
  if (sfd < 0)
    return -1;
  /* SEND_MSG operation format */
  //  1 byte opcode
  //  4 bytes topic name len (network order)(null term counted) = N
  //  4 bytes message length (network order) = M
  //  N bytes topics
  //  M bytes message

  uint8_t op = OP_SEND_MSG;

  size_t topic_len = strlen(topic);

  uint32_t topic_len_net = htonl(topic_len + 1);
  uint32_t msg_len_net = htonl(msg_size);

  struct iovec iov[5];
  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, 4, &msg_len_net);
  iove_setup(iov, 3, topic_len + 1, topic);
  iove_setup(iov, 4, msg_size, msg);

  if(writev(sfd, iov, 5) < 0)
    return -1;

  // Receive response
  int result;
  if(recv(sfd, &result, 4, MSG_WAITALL) <= 0)
    return -1;
  result = ntohl(result);
  return result;
}
// Devuelve la longitud del mensaje almacenado en ese offset del tema indicado
// y un valor negativo en caso de error.
int msg_length(char *topic, int offset)
{
  size_t topic_len = strlen(topic);
  if(topic_len >= 216)
    return -1;

  int sfd = ensure_connected();
  if (sfd < 0)
    return -1;
  /* MSG_LEN format */
  //  1 byte opcode
  //  4 bytes topic len (network order) = N
  //  4 bytes offset(network order)
  //  N bytes topic (with null termination)
  uint8_t op = OP_MSG_LEN;
  uint32_t offset_net = htonl(offset);

  uint32_t topic_len_net = htonl(topic_len+1);

  struct iovec iov[4];

  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, 4, &offset_net);
  iove_setup(iov, 3, topic_len+1, topic);

  if(writev(sfd, iov, 4) < 0)
    return -1;

  // Receive response
  // 4 bytes size(network order), negative for error
  int msg_size;

  if(recv(sfd, &msg_size, 4, MSG_WAITALL) <= 0)
    return -1;;

  msg_size = ntohl(msg_size);

  return msg_size;
}
// Obtiene el último offset asociado a un tema en el broker, que corresponde
// al del último mensaje enviado más uno y, dado que los mensajes se
// numeran desde 0, coincide con el número de mensajes asociados a ese tema.
// Devuelve ese offset si OK y un valor negativo en caso de error.
int end_offset(char *topic)
{
  size_t topic_len = strlen(topic);
  if(topic_len >= 216)
    return -1;
  int sfd = ensure_connected();
  if(sfd < 0)
    return -1;
  // END_OFFSET format:
  //  1 byte opcode
  //  4 bytes topic len(network order) = N
  //  N bytes topic
  struct iovec iov[3];

  uint8_t op = OP_END_OFF;
  uint32_t topic_len_net = htonl(topic_len + 1);

  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, topic_len + 1, topic);

  if(writev(sfd, iov, 3) < 0)
    return -1;

  // Receive reponse
  //  4 bytes: end offset, negative if error
  int end_offset;

  if(recv(sfd, &end_offset, 4, MSG_WAITALL) <= 0)
    return -1;

  end_offset = ntohl(end_offset);

  return end_offset;
}

// TERCERA FASE: SUBSCRIPCIÓN

// Se suscribe al conjunto de temas recibidos. No permite suscripción
// incremental: hay que especificar todos los temas de una vez.
// Si un tema no existe o está repetido en la lista simplemente se ignora.
// Devuelve el número de temas a los que realmente se ha suscrito
// y un valor negativo solo si ya estaba suscrito a algún tema.
int subscribe(int ntopics, char **topics)
{
  return 0;
}

// Se da de baja de todos los temas suscritos.
// Devuelve 0 si OK y un valor negativo si no había suscripciones activas.
int unsubscribe(void)
{
  return 0;
}

// Devuelve el offset del cliente para ese tema y un número negativo en
// caso de error.
int position(char *topic)
{
  return 0;
}

// Modifica el offset del cliente para ese tema.
// Devuelve 0 si OK y un número negativo en caso de error.
int seek(char *topic, int offset)
{
  return 0;
}

// CUARTA FASE: LEER MENSAJES

// Obtiene el siguiente mensaje destinado a este cliente; los dos parámetros
// son de salida.
// Devuelve el tamaño del mensaje (0 si no había mensaje)
// y un número negativo en caso de error.
int poll(char **topic, void **msg)
{
  return 0;
}

// QUINTA FASE: COMMIT OFFSETS

// Cliente guarda el offset especificado para ese tema.
// Devuelve 0 si OK y un número negativo en caso de error.
int commit(char *client, char *topic, int offset)
{
  return 0;
}

// Cliente obtiene el offset guardado para ese tema.
// Devuelve el offset y un número negativo en caso de error.
int commited(char *client, char *topic)
{
  return 0;
}
