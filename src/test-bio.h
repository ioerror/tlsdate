/*
 * test-bio.h - test BIO that stores reads/writes
 *
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TEST_BIO_H
#define TEST_BIO_H

#include <openssl/bio.h>

BIO *BIO_new_test();

size_t BIO_test_output_left (BIO *b);
size_t BIO_test_get_output (BIO *b, unsigned char *buf, size_t bufsz);
void BIO_test_add_input (BIO *b, const unsigned char *buf, size_t bufsz);

#endif /* !TEST_BIO_H */
