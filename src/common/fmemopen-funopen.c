/*
 * POSIX 2008 fmemopen(3) implemented in terms of BSD funopen(3)
 */

/*
 * Copyright (c) 2013 Taylor R. Campbell
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define	_BSD_SOURCE
#define	_NETBSD_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmemopen.h"

struct fmem_cookie {
  void *fmc_buffer;
  size_t fmc_index;
  size_t fmc_limit;
};

static int
fmem_read(void *cookie, char *buffer, int n)
{
  struct fmem_cookie *const fmc = cookie;

  if (n < 0) {                  /* paranoia */
    errno = EINVAL;
    return -1;
  }

  if (n > (fmc->fmc_limit - fmc->fmc_index))
    n = (fmc->fmc_limit - fmc->fmc_index);

  (void)memcpy(buffer, (char *)fmc->fmc_buffer + fmc->fmc_index, n);
  fmc->fmc_index += n;
  return n;
}

static int
fmem_write(void *cookie, const char *buffer, int n)
{
  struct fmem_cookie *const fmc = cookie;

  if (n < 0) {                  /* paranoia */
    errno = EINVAL;
    return -1;
  }

  if (n > (fmc->fmc_limit - fmc->fmc_index))
    n = (fmc->fmc_limit - fmc->fmc_index);

  (void)memcpy((char *)fmc->fmc_buffer + fmc->fmc_index, buffer, n);
  fmc->fmc_index += n;
  return n;
}

static fpos_t
fmem_seek(void *cookie, fpos_t offset, int cmd)
{
  struct fmem_cookie *const fmc = cookie;

  switch (cmd) {
  case SEEK_SET:
    if ((offset < 0) || (fmc->fmc_limit < offset))
      goto einval;
    fmc->fmc_index = offset;
    return 0;

  case SEEK_CUR:
    if (offset < 0) {
      /* Assume two's-complement arithmetic.  */
      if ((offset == ~(fpos_t)0) || (-offset > fmc->fmc_index))
        goto einval;
    } else {
      if (offset > (fmc->fmc_limit - fmc->fmc_index))
        goto einval;
    }
    fmc->fmc_index += offset;
    return 0;

  case SEEK_END:
      /* Assume two's-complement arithmetic.  */
    if ((offset >= 0) || (offset == ~(fpos_t)0) || (fmc->fmc_limit < -offset))
      goto einval;
    fmc->fmc_index = (fmc->fmc_limit + offset);
    return 0;

  default:
    goto einval;
  }

einval:
  errno = EINVAL;
  return -1;
}

static int
fmem_close(void *cookie)
{
  struct fmem_cookie *const fmc = cookie;

  free(fmc);

  return 0;
}

FILE *
fmemopen(void *buffer, size_t len, const char *mode)
{
  struct fmem_cookie *fmc;
  FILE *file;

  fmc = malloc(sizeof(*fmc));
  if (fmc == NULL)
    goto fail0;

  (void)memset(fmc, 0, sizeof(*fmc));
  fmc->fmc_buffer = buffer;
  fmc->fmc_index = 0;
  fmc->fmc_limit = len;

  file = funopen(fmc, &fmem_read, &fmem_write, &fmem_seek, &fmem_close);
  if (file == NULL)
    goto fail1;

  return file;

fail1:
  free(fmc);
fail0:
  return NULL;
}
