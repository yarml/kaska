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

static map *sm = 0; // subscription map
static map_position *sm_pos;

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

  if (writev(sfd, iov, 3) < 0)
    return -1;
  // Receive response
  uint8_t result;
  if (recv(sfd, &result, sizeof(result), MSG_WAITALL) <= 0)
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
  if (write(sfd, &op, 1) < 0)
    return -1;

  // Receive response
  // 4 bytes: Number of topics (network order)
  uint32_t ntopics_net;
  if (recv(sfd, &ntopics_net, 4, MSG_WAITALL) <= 0)
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

  if (writev(sfd, iov, 5) < 0)
    return -1;

  // Receive response
  int result;
  if (recv(sfd, &result, 4, MSG_WAITALL) <= 0)
    return -1;
  result = ntohl(result);
  return result;
}
// Devuelve la longitud del mensaje almacenado en ese offset del tema indicado
// y un valor negativo en caso de error.
int msg_length(char *topic, int offset)
{
  size_t topic_len = strlen(topic);
  if (topic_len >= 216)
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

  uint32_t topic_len_net = htonl(topic_len + 1);

  struct iovec iov[4];

  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, 4, &offset_net);
  iove_setup(iov, 3, topic_len + 1, topic);

  if (writev(sfd, iov, 4) < 0)
    return -1;

  // Receive response
  // 4 bytes size(network order), negative for error
  int msg_size;

  if (recv(sfd, &msg_size, 4, MSG_WAITALL) <= 0)
    return -1;
  ;

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
  if (topic_len >= 216)
    return -1;
  int sfd = ensure_connected();
  if (sfd < 0)
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

  if (writev(sfd, iov, 3) < 0)
    return -1;

  // Receive reponse
  //  4 bytes: end offset, negative if error
  int end_offset;

  if (recv(sfd, &end_offset, 4, MSG_WAITALL) <= 0)
    return -1;

  end_offset = ntohl(end_offset);

  return end_offset;
}

// TERCERA FASE: SUBSCRIPCIÓN

// Subscribe to the set of received topics. does not allow subscription
// incremental: all themes must be specified at once.
// If a topic does not exist or is repeated in the list, it is simply ignored.
// Returns the number of topics actually subscribed to
// and a negative value only if you were already subscribed to a topic.
int subscribe(int ntopics, char **topics)
{
  if (sm) // We already subscribed to topics, can't subscribe again
    return -1;
  sm = map_create(key_string, 0); // No locking
  sm_pos = map_alloc_position(sm);
  int actually_subs = 0; // Does not count duplicates or non existant
  for (int i = 0; i < ntopics; ++i)
  {
    if (strlen(topics[i]) >= 216) // If too long, don't bother looking it up
      continue;
    int err = 0;
    // First we check if the topic is already added, if that's the case
    // don't bother asking the broker about its end offset
    map_get(sm, topics[i], &err);
    if (!err)
      continue;
    int eoff = end_offset(topics[i]);
    if (eoff < 0)
      continue;
    char *dup_topic = strdup(topics[i]); // free() in release_subscription
    int *poff = malloc(sizeof(int));     // free() in release_subscription
    *poff = eoff;
    map_put(sm, dup_topic, poff);
    ++actually_subs;
  }
  return actually_subs;
}

static void release_subscription(void *key, void *value)
{
  free(value);
  free(key);
}

// Se da de baja de todos los temas suscritos.
// Devuelve 0 si OK y un valor negativo si no había suscripciones activas.
int unsubscribe(void)
{
  if (!sm) // Can't unsubscribe if not subscribed already :)
    return -1;
  map_free_position(sm_pos);
  map_destroy(sm, release_subscription);
  sm = 0; // subscribe can be called again
  return 0;
}

// Devuelve el offset del cliente para ese tema y un número negativo en
// caso de error.
int position(char *topic)
{
  if (!sm)
    return -1;
  int err = 0;
  int *poff = map_get(sm, topic, &err);
  if (err)
    return -1;
  return *poff;
}

// Modifica el offset del cliente para ese tema.
// Devuelve 0 si OK y un número negativo en caso de error.
int seek(char *topic, int offset)
{
  if (!sm)
    return -1;
  int err = 0;
  int *poff = map_get(sm, topic, &err);
  if (err)
    return -1;
  *poff = offset;
  return 0;
}

// CUARTA FASE: LEER MENSAJES

// Get the following message for this client; the two parameters
// are output.
// Returns the size of the message (0 if there was no message)
// and a negative number on error.
int poll(char **topic, void **msg)
{
  if (!sm)
    return -1;
  int sfd = ensure_connected();
  map_iter *it = map_iter_init(sm, sm_pos);
  for (; it && map_iter_has_next(it); map_iter_next(it))
  {
    char *ctopic;
    int *poff;
    map_iter_value(it, (void const **) &ctopic, (void **) &poff);

    // Send a poll request
    // POLL format:
    //  1 byte: opcode
    //  4 bytes: topic len = N
    //  4 bytes: offset
    //  N bytes: topic(with NULL term)

    uint8_t op = OP_POLL;
    size_t ctopic_len = strlen(ctopic);

    uint32_t topic_len_net = htonl(ctopic_len+1);
    uint32_t offset_net = htonl(*poff);

    struct iovec iov[4];

    iove_setup(iov, 0, 1, &op);
    iove_setup(iov, 1, 4, &topic_len_net);
    iove_setup(iov, 2, 4, &offset_net);
    iove_setup(iov, 3, ctopic_len + 1, ctopic);

    if (writev(sfd, iov, 4) < 0)
    {
      sm_pos = map_iter_exit(it);
      return -1;
    }

    // broker response is of the format:
    // 4 bytes: msg len, 0 means message at offset for topic does not exist = N
    // N bytes: msg
    uint32_t msg_len;
    if (recv(sfd, &msg_len, 4, MSG_WAITALL) <= 0)
    {
      sm_pos = map_iter_exit(it);
      return -1;
    }
    msg_len = ntohl(msg_len);

    printf("Receiving a message of length: %u\n", msg_len);

    if (!msg_len)
      continue;
    void *msgbuf = malloc(msg_len);
    if (recv(sfd, msgbuf, msg_len, MSG_WAITALL) <= 0)
    {
      free(msgbuf);
      sm_pos = map_iter_exit(it);
      return -1;
    }
    *topic = strdup(ctopic);
    *msg = msgbuf;
    ++*poff; // Increment offset so that next time we read from this topic
             // We read the next message from the broker
    sm_pos = map_iter_exit(it);
    return msg_len;
  }
  sm_pos = map_iter_exit(it);
  return 0;
}

// QUINTA FASE: COMMIT OFFSETS

// Cliente guarda el offset especificado para ese tema.
// Devuelve 0 si OK y un número negativo en caso de error.

// In the library, you would need to send 2 strings: the client and the topic,
// along with the operation code and the offset. As for the broker, you would
// need to create the corresponding subdirectory for the client and the
// associated file for the topic if they don't already exist, and write the
// offset to the file.

int commit(char *client, char *topic, int offset)
{
  size_t topic_len = strlen(topic);
  size_t client_len = strlen(client);
  if(topic_len >= 216)
    return -1;

  // The broker would reject these anyway, so don't bother sending the request
  if(client[0] == '.' || strchr(client, '/'))
    return -1;

  int sfd = ensure_connected();
  if(sfd < 0)
    return -1;

  // COMMIT op format
  //  1 byte: opcode
  //  4 bytes: topic len = N
  //  4 bytes: client len = M
  //  4 bytes: offset
  //  N bytes: topic (with Null term)
  //  M bytes: client(with Null term)

  uint8_t op = OP_COMMIT;
  uint32_t topic_len_net = htonl(topic_len+1);
  uint32_t client_len_net = htonl(client_len+1);
  uint32_t offset_net = htonl(offset);

  struct iovec iov[6];

  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, 4, &client_len_net);
  iove_setup(iov, 3, 4, &offset_net);
  iove_setup(iov, 4, topic_len+1, topic);
  iove_setup(iov, 5, client_len+1, client);

  if(writev(sfd, iov, 6) < 0)
    return -1;

  // Response is just one byte status
  uint8_t status;
  if(recv(sfd, &status, 1, MSG_WAITALL) <= 0)
    return -1;
  return status;
}

// Cliente obtiene el offset guardado para ese tema.
// Devuelve el offset y un número negativo en caso de error.
int commited(char *client, char *topic)
{
  size_t topic_len = strlen(topic);
  size_t client_len = strlen(client);
  if(topic_len >= 216)
    return -1;

  // The broker would reject these anyway, so don't bother sending the request
  if(client[0] == '.' || strchr(client, '/'))
    return -1;

  int sfd = ensure_connected();
  if(sfd < 0)
    return -1;

  // COMMITED op format
  //  1 byte: opcode
  //  4 bytes: topic len = N
  //  4 bytes: client len = M
  //  N bytes: topic (with Null term)
  //  M bytes: client(with Null term)

  uint8_t op = OP_COMMITED;
  uint32_t topic_len_net = htonl(topic_len+1);
  uint32_t client_len_net = htonl(client_len+1);

  struct iovec iov[5];

  iove_setup(iov, 0, 1, &op);
  iove_setup(iov, 1, 4, &topic_len_net);
  iove_setup(iov, 2, 4, &client_len_net);
  iove_setup(iov, 3, topic_len+1, topic);
  iove_setup(iov, 4, client_len+1, client);

  if(writev(sfd, iov, 5) < 0)
    return -1;

  // Response is just 4 bytes offset, negative if error
  int offset;
  if(recv(sfd, &offset, 4, MSG_WAITALL) <= 0)
    return -1;
  offset = ntohl(offset);
  return offset;
}
