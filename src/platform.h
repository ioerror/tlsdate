/*
 * platform.h - platform handlers for event loops
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PLATFORM_H_
#define PLATFORM_H_
#include "config.h"

struct state;

#ifdef HAVE_CROS
int platform_init_cros (struct state *state);
#else
static inline int platform_init_cros (struct state *state)
{
  return 0;
}
#endif  /* HAVE_CROS */
#endif  /* PLATFORM_H_ */
