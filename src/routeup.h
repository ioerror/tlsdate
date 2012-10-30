/*
 * routeup.h - routeup library interface
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Call routeup_setup() to initialize a routeup context, then call
 * routeup_once() until it returns nonzero (indicating an error) or until
 * you're no longer interested in route changes. Call routeup_teardown()
 * when you're done with an routeup context.
 */

#ifndef ROUTEUP_H
#define ROUTEUP_H

struct routeup {
  int netlinkfd;  /* AF_NETLINK event socket */
};

int routeup_setup(struct routeup *ifc);
int routeup_once(struct routeup *ifc, unsigned int timeout);
void routeup_teardown(struct routeup *ifc);

#endif /* !ROUTEUP_H */
