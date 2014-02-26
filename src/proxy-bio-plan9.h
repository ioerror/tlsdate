/*
 * proxy-bio.h - BIO layer for transparent proxy connections
 *
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PROXY_BIO_H
#define PROXY_BIO_H

#include <inttypes.h>

#include <openssl/bio.h>

#include "util-plan9.h"

struct proxy_ctx {
  char *host;
  uint16_t port;
  int connected;
  int (*connect)(BIO *b);
};

BIO *BIO_new_proxy();

/* These do not take ownership of their string arguments. */
int BIO_proxy_set_type(BIO *b, const char *type);
int BIO_proxy_set_host(BIO *b, const char *host);
void BIO_proxy_set_port(BIO *b, uint16_t port);

#endif /* !PROXY_BIO_H */
