/*
 * proxy-polarssl.h - PolarSSL layer for transparent proxy connections
 *
 * Based on proxy-bio.c - Original copyright (c) 2012 The Chromium OS Authors.
 * 
 * This file was adapted by Paul Bakker <p.j.bakker@offspark.com>
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PROXY_POLARSSL_H
#define PROXY_POLARSSL_H

#include <stdint.h>

typedef struct _proxy_polarssl_ctx proxy_polarssl_ctx;

struct _proxy_polarssl_ctx {
  char *host;
  uint16_t port;
  int connected;

  int (*f_recv)(void *, unsigned char *, size_t);
  int (*f_send)(void *, const unsigned char *, size_t);
  int (*f_connect)(proxy_polarssl_ctx *);

  void *p_recv;               /*!< context for reading operations   */
  void *p_send;               /*!< context for writing operations   */
};

int proxy_polarssl_init(proxy_polarssl_ctx *proxy);
int proxy_polarssl_free(proxy_polarssl_ctx *ctx);

void proxy_polarssl_set_bio(proxy_polarssl_ctx *ctx,
                       int (*f_recv)(void *, unsigned char *, size_t), void *p_recv,
                       int (*f_send)(void *, const unsigned char *, size_t), void *p_send);
int proxy_polarssl_set_scheme(proxy_polarssl_ctx *ctx, const char *scheme);
int proxy_polarssl_set_host(proxy_polarssl_ctx *ctx, const char *host);
void proxy_polarssl_set_port(proxy_polarssl_ctx *ctx, uint16_t port);

int proxy_polarssl_recv(void *ctx, unsigned char *data, size_t len);
int proxy_polarssl_send(void *ctx, const unsigned char *data, size_t len);

#endif /* !PROXY_POLARSSL_H */
