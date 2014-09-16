/*
 * proxy-bio-unittest.c - proxy-bio unit tests
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if defined(__linux__)
#include <alloca.h>
#endif

#include "src/proxy-bio.h"
#include "src/test-bio.h"
#include "src/test_harness.h"
#include "src/tlsdate.h"

FIXTURE (test_bio)
{
  BIO *test;
};

FIXTURE_SETUP (test_bio)
{
  self->test = BIO_new_test();
  ASSERT_NE (NULL, self->test);
}

FIXTURE_TEARDOWN (test_bio)
{
  BIO_free (self->test);
}

BIO *proxy_bio (BIO *test, const char *type)
{
  BIO *proxy = BIO_new_proxy();
  BIO_proxy_set_type (proxy, type);
  BIO_proxy_set_host (proxy, kTestHost);
  BIO_proxy_set_port (proxy, TEST_PORT);
  BIO_push (proxy, test);
  return proxy;
}

int need_out_bytes (BIO *test, const unsigned char *out, size_t sz)
{
  unsigned char *buf = malloc (sz);
  size_t i;
  int result;
  if (!buf)
    return 1;
  if (BIO_test_output_left (test) <  sz)
    {
      fprintf (TH_LOG_STREAM, "not enough output: %d < %d\n",
               (int) BIO_test_output_left (test), (int) sz);
      free (buf);
      return 2;
    }
  if (BIO_test_get_output (test, buf, sz) != sz)
    {
      free (buf);
      return 3;
    }
  if (memcmp (buf, out, sz))
    {
      for (i = 0; i < sz; i++)
        {
          if (buf[i] != out[i])
            fprintf (TH_LOG_STREAM,
                     "mismatch %d %02x %02x\n", (int) i,
                     buf[i], out[i]);
        }
    }
  result = memcmp (buf, out, sz);
  free (buf);
  return result;
}

int need_out_byte (BIO *test, unsigned char out)
{
  unsigned char c;
  if (BIO_test_output_left (test) < 1)
    return 1;
  if (BIO_test_get_output (test, &c, 1) != 1)
    return 2;
  return c != out;
}

int get_bytes (BIO *test, unsigned char *buf, size_t sz)
{
  return BIO_test_get_output (test, buf, sz);
}

void put_bytes (BIO *test, const unsigned char *buf, size_t sz)
{
  BIO_test_add_input (test, buf, sz);
}

void put_byte (BIO *test, char c)
{
  BIO_test_add_input (test, (unsigned char *) &c, 1);
}

unsigned const char kSocks4ARequest[] =
{
  0x04, /* socks4 */
  0x01, /* tcp stream */
  (TEST_PORT & 0xff00) >> 8,
  TEST_PORT & 0xff,
  0x00, 0x00, 0x00, 0x01, /* bogus IP */
  0x00, /* userid */
  TEST_HOST, 0x00 /* null-terminated host */
};

TEST_F (test_bio, socks4a_success)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  unsigned const char kReply[] =
  {
    0x00, /* null byte */
    0x5a, /* success */
    (TEST_PORT & 0xff00) >> 8,  /* port high */
    TEST_PORT & 0xff, /* port low */
    0x00, 0x00, 0x00, 0x00  /* bogus IP */
  };
  BIO *proxy = proxy_bio (self->test, "socks4a");
  put_bytes (self->test, kReply, sizeof (kReply));
  EXPECT_EQ (4, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks4ARequest,
                                sizeof (kSocks4ARequest)));
  EXPECT_EQ (0, need_out_bytes (self->test, kTestInput,
                                sizeof (kTestInput)));
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

TEST_F (test_bio, socks4a_fail)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  unsigned const char kReply[] =
  {
    0x00, /* null byte */
    0x5b, /* fail */
    (TEST_PORT & 0xff00) >> 8,  /* port high */
    TEST_PORT & 0xff, /* port low */
    0x00, 0x00, 0x00, 0x00  /* bogus IP */
  };
  BIO *proxy = proxy_bio (self->test, "socks4a");
  put_bytes (self->test, kReply, sizeof (kReply));
  EXPECT_EQ (0, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks4ARequest,
                                sizeof (kSocks4ARequest)));
  /* We shouldn't have written any payload */
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

unsigned const char kSocks5AuthRequest[] =
{
  0x05, /* socks5 */
  0x01, /* one auth method */
  0x00  /* no auth */
};

unsigned const char kSocks5AuthReply[] =
{
  0x05, /* socks5 */
  0x00, /* no auth */
};

unsigned const char kSocks5ConnectRequest[] =
{
  0x05, /* socks5 */
  0x01, /* tcp stream */
  0x00, /* reserved 0x00 */
  0x03, /* domain name */
  TEST_HOST_SIZE, /* hostname with length prefix */
  TEST_HOST,
  (TEST_PORT & 0xff00) >> 8,
  TEST_PORT & 0xff
};

unsigned const char kSocks5ConnectReply[] =
{
  0x05, /* socks5 */
  0x00, /* success */
  0x00, /* reserved 0x00 */
  0x03, /* domain name */
  TEST_HOST_SIZE, /* hostname with length prefix */
  TEST_HOST,
  (TEST_PORT & 0xff00) >> 8,
  TEST_PORT & 0xff
};

TEST_F (test_bio, socks5_success)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  BIO *proxy = proxy_bio (self->test, "socks5");
  put_bytes (self->test, kSocks5AuthReply, sizeof (kSocks5AuthReply));
  put_bytes (self->test, kSocks5ConnectReply, sizeof (kSocks5ConnectReply));
  EXPECT_EQ (4, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks5AuthRequest,
                                sizeof (kSocks5AuthRequest)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks5ConnectRequest,
                                sizeof (kSocks5ConnectRequest)));
  EXPECT_EQ (0, need_out_bytes (self->test, kTestInput,
                                sizeof (kTestInput)));
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

TEST_F (test_bio, socks5_auth_fail)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  unsigned const char kAuthFail[] =
  {
    0x05,
    0xff,
  };
  BIO *proxy = proxy_bio (self->test, "socks5");
  put_bytes (self->test, kAuthFail, sizeof (kAuthFail));
  EXPECT_EQ (0, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks5AuthRequest,
                                sizeof (kSocks5AuthRequest)));
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

TEST_F (test_bio, socks5_connect_fail)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  unsigned const char kConnectFail[] =
  {
    0x05,
    0x01,
    0x00,
    0x03,
    TEST_HOST_SIZE,
    TEST_HOST,
    (TEST_PORT & 0xff00) >> 8,
    TEST_PORT & 0xff
  };
  BIO *proxy = proxy_bio (self->test, "socks5");
  put_bytes (self->test, kSocks5AuthReply, sizeof (kSocks5AuthReply));
  put_bytes (self->test, kConnectFail, sizeof (kConnectFail));
  EXPECT_EQ (0, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks5AuthRequest,
                                sizeof (kSocks5AuthRequest)));
  EXPECT_EQ (0, need_out_bytes (self->test, kSocks5ConnectRequest,
                                sizeof (kSocks5ConnectRequest)));
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

TEST_F (test_bio, http_success)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  BIO *proxy = proxy_bio (self->test, "http");
  char kConnectRequest[1024];
  char kConnectResponse[] = "HTTP/1.0 200 OK\r\n"
                            "Uninteresting-Header: foobar\r\n"
                            "Another-Header: lol\r\n"
                            "\r\n";
  snprintf (kConnectRequest, sizeof (kConnectRequest),
            "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
            kTestHost, TEST_PORT, kTestHost, TEST_PORT);
  put_bytes (self->test, (unsigned char *) kConnectResponse,
             strlen (kConnectResponse));
  EXPECT_EQ (4, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test,
                                (unsigned char *) kConnectRequest,
                                strlen (kConnectRequest)));
  EXPECT_EQ (0, need_out_bytes (self->test, kTestInput,
                                sizeof (kTestInput)));
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

TEST_F (test_bio, http_error)
{
  unsigned const char kTestInput[] = { 0xde, 0xad, 0xbe, 0xef };
  BIO *proxy = proxy_bio (self->test, "http");
  char kConnectRequest[1024];
  char kConnectResponse[] = "HTTP/1.0 403 NO U\r\n"
                            "Uninteresting-Header: foobar\r\n"
                            "Another-Header: lol\r\n"
                            "\r\n";
  snprintf (kConnectRequest, sizeof (kConnectRequest),
            "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
            kTestHost, TEST_PORT, kTestHost, TEST_PORT);
  put_bytes (self->test, (unsigned char *) kConnectResponse,
             strlen (kConnectResponse));
  EXPECT_EQ (0, BIO_write (proxy, kTestInput, sizeof (kTestInput)));
  EXPECT_EQ (0, need_out_bytes (self->test,
                                (unsigned char *) kConnectRequest,
                                strlen (kConnectRequest)));
  EXPECT_EQ (0, BIO_test_output_left (self->test));
}

TEST_HARNESS_MAIN
