/*
 * Incluya en este fichero todas las implementaciones que pueden
 * necesitar compartir el broker y la biblioteca, si es que las hubiera.
 */
#include <sys/uio.h>
#include "comun.h"

void iove_setup(struct iovec *iov, size_t index, size_t len, void *base)
{
  iov[index].iov_base = base;
  iov[index].iov_len = len;
}
