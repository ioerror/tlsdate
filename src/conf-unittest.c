/*
 * conf-unittest.c - config parser unit tests
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/conf.h"
#include "src/test_harness.h"

#ifdef HAVE_ANDROID_SYSTEM
#include "src/common/fmemopen.h"
#endif

#ifndef HAVE_FMEMOPEN
#include "src/common/fmemopen.h"
#endif

FILE *fopenstr (const char *str)
{
  /* strlen(str) instead of strlen(str) + 1 because files shouldn't appear
   * null-terminated. Cast away constness because we're in read mode, but the
   * fmemopen prototype has no way to express that. */
  return fmemopen ( (char *) str, strlen (str), "r");
}

TEST (parse_empty)
{
  /* can't do a truly empty file - fmemopen() combusts */
  FILE *f = fopenstr ("\n");
  ASSERT_NE (NULL, f);
  struct conf_entry *e = conf_parse (f);
  EXPECT_NULL (e);
  conf_free (e);
}

TEST (parse_basic)
{
  FILE *f = fopenstr ("foo bar\nbaz quxx\n");
  ASSERT_NE (NULL, f);
  struct conf_entry *e = conf_parse (f);
  ASSERT_NE (NULL, e);
  EXPECT_STREQ (e->key, "foo");
  EXPECT_STREQ (e->value, "bar");
  ASSERT_NE (NULL, e->next);
  EXPECT_STREQ (e->next->key, "baz");
  EXPECT_STREQ (e->next->value, "quxx");
  ASSERT_NULL (e->next->next);
  conf_free (e);
}

TEST (parse_novalue)
{
  FILE *f = fopenstr ("abcdef\n");
  ASSERT_NE (NULL, f);
  struct conf_entry *e = conf_parse (f);
  ASSERT_NE (NULL, e);
  EXPECT_STREQ (e->key, "abcdef");
  EXPECT_NULL (e->value);
  EXPECT_NULL (e->next);
  conf_free (e);
}

TEST (parse_whitespace)
{
  FILE *f = fopenstr ("         fribble		  grotz  \n");
  ASSERT_NE (NULL, f);
  struct conf_entry *e = conf_parse (f);
  ASSERT_NE (NULL, e);
  EXPECT_STREQ (e->key, "fribble");
  EXPECT_STREQ (e->value, "grotz  ");
  EXPECT_NULL (e->next);
  conf_free (e);
}

TEST (parse_comment)
{
  FILE *f = fopenstr ("#foo bar\nbaz quxx\n");
  ASSERT_NE (NULL, f);
  struct conf_entry *e = conf_parse (f);
  ASSERT_NE (NULL, e);
  EXPECT_STREQ (e->key, "baz");
  EXPECT_STREQ (e->value, "quxx");
  EXPECT_NULL (e->next);
  conf_free (e);
}

TEST_HARNESS_MAIN
