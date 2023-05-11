As we examined in the theoretical part of the course, publisher/subscriber
systems with a pull mode of operation and persistent event storage, i.e. with a
streaming scheme, provides an appropriate architecture for many of the scenarios
presented. Within these architectures, Apache Kafka is the main platform and it
is on. This system is the focus of this practical project, whose name pays
homage to this popular platform, while serving as a reminder of the fallibility
of software. It is obviously a very reduced version of this complex software
component, which leaves out many of its functionalities (the use of several
brokers with replication and partitioning, request and response packaging,
key usage, message persistence, consumer groups, message retrieval by timestamp,
etc.), but we believe that it allows us to appreciate the type of functionality
of this type of system and to better understand its internal mode of operation.


Although we have already studied it in the theoretical part of the course, we
are going to recall the most important points of this type of system, assuming
a mode of operation similar to Kafka's:

- The broker (brokers in Kafka's case) stores messages/events sent by
  producers/editors to different topics.
- In Kafka, a consumer/subscriber stores the topics they subscribe to and the
  last message/event they read in each of those topics (the offset). This part
  of the system state is therefore not stored on the server/broker, but in the
  client's library.
- It works in "pull" mode: a consumer/subscriber asks the broker for a new
  message indicating the themes to which he subscribes and his offset for each
  of these themes. When a consumer/subscriber subscribes to a topic, their
  initial offset is such that they will only be able to see posts sent to that
  topic from that point on.
- A consumer/subscriber can change their offset in a topic to receive messages
  prior to their subscription or to receive a message again.
- To allow a consumer/subscriber to not always be active and to pick up where it
  left off when it restarts, its offsets can be stored so persisted in the
  broker and recovered on restart.

In the following, a number of requirements are specified, which must be
satisfied by the developed practice:
- The practice should work both locally and remotely.
- Regarding the communication technologies used in practice, the program will be
  programmed in C, stream sockets will be used and a heterogeneous machines will
  be assumed.
- A schema will be used with a single process acting as a broker providing
  spatial and temporal decoupling between publishers (producers in the
  terminology Kafka) and subscribers (consumers in Kafka terminology). Using a
  streaming type scheme, the broker will be responsible for storing events.
- The broker will provide a concurrent service based on the dynamic creation of
  threads, with each thread responsible for responding to all requests from a
  connection.
- A publisher and/or subscriber process (recall that a process can play both
  roles) will maintain a persistent connection with the broker throughout its
  interaction.
- Given this possible dual role, we will refer to these processes as clients in
  the rest of the document.
- As in the Kafka protocol, the name of a subject is a character string with a
  maximum size of 216-1 bytes, including the terminating null character.
- Messages/events sent may have binary content (for example, they may be an
  image or ciphertext). They therefore cannot be treated as character strings
  and their size must be explicitly known. It should be noted that applications
  using this system will use the scheme of serialization that they deem
  appropriate to send the information they process.
- The system design should not limit the number of themes and clients in the
  system, nor the number of messages stored in the broker.
- Don't forget to correctly process the character strings received by ensuring
  that they end with a null character.
- Zero copy behavior must be ensured both in the treatment of subject names and
  in message content:
    - Copies of these fields cannot be made in the broker.
    - Copies of these fields cannot be made in the client's library
      unless the declaration says so.
    - To avoid transmission fragmentation, clients and broker send all
      information of a request or response, respectively, in one one time.
- Bandwidth usage should be optimized so that the size of the information sent
  is only slightly larger than the sum of the sizes of the fields to be sent.
- To facilitate the development of the practice, an implementation of a data
  type which acts as an iterable map, allowing to associate a value with a key,
  and a type which manages an append-only queue are provided. It is mandatory
  to use these types of data when implementing the practice.

