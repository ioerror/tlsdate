/* conf.h - config file parser */

#ifndef CONF_H
#define CONF_H

#include <stdio.h>

#define CONF_MAX_LINE 16384

struct conf_entry
{
  struct conf_entry *next;
  char *key;
  char *value;
};

struct conf_entry *conf_parse (FILE *f);
void conf_free (struct conf_entry *e);

#endif /* !CONF_H */
