/*
 * Incluya en este fichero todas las definiciones que pueden
 * necesitar compartir el broker y la biblioteca, si es que las hubiera.
 */

#ifndef _COMUN_H
#define _COMUN_H        1

#include <stddef.h>

// All operation codes defined by Kaska
#define OP_CREATE_TOPIC (0x10)
#define OP_NTOPICS (0x11)

#define OP_SEND_MSG (0x20)
#define OP_MSG_LEN (0x21)
#define OP_END_OFF (0x22)

#define OP_POLL (0x40)

// Create Topic result codes
#define OP_CT_SUCCESS (0)
#define OP_CT_EXISTS (1) // topic already exists

// Send message result codes
#define OP_SM_NOTOPIC (-1)
#define OP_SM_FAIL (-2)

// Common functions
void iove_setup(struct iovec *iov, size_t index, size_t len, void *base);

#endif // _COMUN_H
