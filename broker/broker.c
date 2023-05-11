#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include "comun.h"
#include "map.h"
#include "queue.h"

#define BACKLOG (5)

typedef struct THREAD_INFO thread_info;
struct THREAD_INFO
{
  int cfd;
  map *topics;
};

typedef struct MESSAGE message;
struct MESSAGE
{
  size_t len;
  void *base;
};

static int init_server(int port)
{
  int status;
  int reuseaddr_opt = 1;
  int sfd;                 // server file descriptor
  struct sockaddr_in sadr; // server address

  sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sfd < 0)
  {
    perror("socket");
    return -1;
  }
  // Reuse port
  status = setsockopt(
      sfd,
      SOL_SOCKET,
      SO_REUSEADDR,
      &reuseaddr_opt,
      sizeof(reuseaddr_opt));
  if (status < 0)
  {
    perror("setsockopt");
    close(sfd);
    return -2;
  }

  // Init server address struct (sadr)
  sadr.sin_addr.s_addr = INADDR_ANY;
  sadr.sin_port = htons(port);
  sadr.sin_family = AF_INET;

  // Reserve specified port
  status = bind(sfd, (struct sockaddr *)&sadr, sizeof(sadr));
  if (status < 0)
  {
    perror("bind");
    close(sfd);
    return -3;
  }

  // Listen to incomming connection
  status = listen(sfd, BACKLOG);
  if (status < 0)
  {
    perror("listen");
    close(sfd);
    return -4;
  }
  return sfd;
}

void *handle_connection(void *parg_thinf)
{
  thread_info *thinf = parg_thinf;
  int cfd = thinf->cfd;
  map *topics = thinf->topics;

  printf("[%3d] Connection opened\n", cfd);

  while (1)
  {
    uint8_t op;
    // If any of the receives returns <= 0, we know the connection ended
    if (recv(cfd, &op, 1, MSG_WAITALL) <= 0)
      break;
    switch (op)
    {
    case OP_CREATE_TOPIC: // We do not free() msg here, it will be free()d by map_destroy
    {
      // Receive the rest of the message
      // 4 bytes: topic name length (network order) = N
      uint32_t topic_len_net;
      if (recv(cfd, &topic_len_net, 4, MSG_WAITALL) <= 0)
        goto connection_lost;
      uint32_t topic_len = ntohl(topic_len_net); // Null termiination also
                                                 // counted
      // N bytes topic name
      char *topic = malloc(topic_len); // free()d in map_destroy
      if (recv(cfd, topic, topic_len, MSG_WAITALL) <= 0)
        goto connection_lost;

      uint8_t result;
      queue *new_topic_queue = queue_create(1); // Use locks
      if (map_put(topics, topic, new_topic_queue) == -1)
      {
        queue_destroy(new_topic_queue, 0);
        result = OP_CT_EXISTS;
      }
      else
        result = OP_CT_SUCCESS;
      write(cfd, &result, sizeof(result));
      break;
    }
    case OP_NTOPICS:
    {
      // NTOPICS only takes the opcode, no further bytes to read from client
      uint32_t ntopics = map_size(topics);
      uint32_t ntopics_net = htonl(ntopics);
      write(cfd, &ntopics_net, sizeof(ntopics_net));
    }
    break;
    case OP_SEND_MSG: // Client wants to send message to a topic
    {
      // We need to receive the rest of the message
      // First we receive the topic name length(0 term counter)
      // then receive the message length
      // Then receive the topic name length
      // Then receive the message

      uint32_t topic_len_net, msg_len_net;
      if (recv(cfd, &topic_len_net, 4, MSG_WAITALL) <= 0)
        goto connection_lost;
      if (recv(cfd, &msg_len_net, 4, MSG_WAITALL) <= 0)
        goto connection_lost;
      uint32_t topic_len = ntohl(topic_len_net);
      uint32_t msg_len = ntohl(msg_len_net);

      char *topic = malloc(topic_len);
      void *msg = malloc(msg_len);          // free()d in release_message
      message *m = malloc(sizeof(message)); // free()d in release_message

      m->base = msg;
      m->len = msg_len;

      if (recv(cfd, topic, topic_len, MSG_WAITALL) <= 0)
        goto connection_lost;
      if (recv(cfd, msg, msg_len, MSG_WAITALL) <= 0)
        goto connection_lost;
      int result;
      int err = 0;
      queue *tq = map_get(topics, topic, &err);
      if (err == -1)
      {
        result = OP_SM_NOTOPIC;
        free(msg);
        free(m);
      }
      else
      {
        // tq is a pointer to the target queue
        result = queue_append(tq, m);
        if(result < 0)
        {
          free(msg);
          free(m);

        }
      }
      result = htonl(result);
      free(topic);
      write(cfd, &result, 4);
    }
    break;
    case OP_MSG_LEN:
    {
      // The rest of the message
      // 4 bytes topic len = N
      // 4 bytes offset
      // N bytes topic
      uint32_t topic_len;
      if (recv(cfd, &topic_len, 4, MSG_WAITALL) <= 0)
        goto connection_lost;
      topic_len = ntohl(topic_len);
      uint32_t offset;
      if (recv(cfd, &offset, 4, MSG_WAITALL) <= 0)
        goto connection_lost;
      offset = htonl(offset);

      int result;

      char *topic = malloc(topic_len);
      if (recv(cfd, topic, topic_len, MSG_WAITALL) <= 0)
        goto connection_lost;

      int err = 0;
      queue *tq = map_get(topics, topic, &err);
      if (err == -1)
        result = -1;
      else
      {
        err = 0;
        message *m = queue_get(tq, offset, &err);
        if (err == -1)
          result = 0;
        else
          result = m->len;
      }
      free(topic);
      result = htonl(result);
      write(cfd, &result, 4);
    }
    break;
    case OP_END_OFF:
    {
      int err = 0;
      // Receive topic len and topic
      uint32_t topic_len;
      if (recv(cfd, &topic_len, 4, MSG_WAITALL) <= 0)
        goto connection_lost;
      topic_len = ntohl(topic_len);

      char *topic = malloc(topic_len);
      if (recv(cfd, topic, topic_len, MSG_WAITALL) <= 0)
        goto connection_lost;

      // Send result
      int result;
      queue *tq = map_get(topics, topic, &err);
      if (err == -1)
        result = -1;
      else
        result = queue_size(tq);
      result = htonl(result);
      free(topic);
      write(cfd, &result, 4);
    }
      break;
    default: // If we receive an invalid opcode, we break the connection
      goto connection_lost;
    }
  }
connection_lost:
  printf("[%3d] Connection closed\n", cfd);
  free(parg_thinf); // The reference servidor.c didn't free the argument, just saying
  close(cfd);
  return 0;
}

void release_message(void *value)
{
  message *m = value;
  free(m->base);
  free(value);
}

void topic_queue_release(void *key, void *value)
{
  queue_destroy(value, release_message);
  free(key);
}

int main(int argc, char **argv)
{
  if (argc != 2 && argc != 3)
  {
    fprintf(stderr, "Usage: %s port [dir_commited]\n", argv[0]);
    return 1;
  }

  int port = atoi(argv[1]);

  // Open server on specified port
  int sfd = init_server(port);
  if (sfd < 0)
  {
    char *err_msg;
    switch (sfd)
    {
    case -1:
      err_msg = "Could not create socket";
      break;
    case -2:
      err_msg = "Could not configure socket";
      break;
    case -3:
      err_msg = "Could not use specified port";
      break;
    case -4:
      err_msg = "Could not listen for incomming connections";
      break;
    default:
      err_msg = "Unknown error";
      break;
    }
    fprintf(stderr, "%s\n", err_msg);
    exit(-sfd);
  }

  // Create a map topic->message queue
  // This map uses locks
  map *topics = map_create(key_string, 1);
  if (!topics)
  {
    perror("map_create");
    close(sfd);
  }

  // Init client thread attributes; all clients have the same
  // attributes for the thread handling them
  pthread_attr_t cth_attrib;
  pthread_attr_init(&cth_attrib); // evita pthread_join
  pthread_attr_setdetachstate(&cth_attrib, PTHREAD_CREATE_DETACHED);

  // Wait for incomming connections
  while (1)
  {
    int cfd;                 // Next client file descriptor
    pthread_t cthid;         // Next client's thread ID
    struct sockaddr_in cadr; // Client address
    socklen_t cadr_sz = sizeof(cadr);
    thread_info *thinf; // Pointer to thread info structure for next client
    int status;

    // Accept next TCP connection request
    cfd = accept(sfd, (struct sockaddr *)&cadr, &cadr_sz);
    if (cfd < 0)
    {
      perror("accept");
      map_destroy(topics, topic_queue_release);
      close(sfd);
      exit(-1);
    }

    // Init client's thread info
    thinf = malloc(sizeof(*thinf));
    if (!thinf)
    {
      perror("malloc");
      map_destroy(topics, topic_queue_release);
      close(sfd);
      exit(-2);
    }
    thinf->cfd = cfd;
    thinf->topics = topics;

    status = pthread_create(&cthid, &cth_attrib, handle_connection, thinf);
    if (status)
    {
      perror("pthread_create");
      map_destroy(topics, topic_queue_release);
      free(thinf);
      close(sfd);
      exit(-3);
    }
  }
}
