/* conf.c - config file parser */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strchrnul */
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/conf.h"

void strip_newlines(char *line)
{
  *strchrnul(line, '\n') = '\0';
  *strchrnul(line, '\r') = '\0';
}

char *eat_whitespace(char *line)
{
  while (isspace(*line))
    line++;
  return line;
}

int is_ignored_line(char *line)
{
  return !*line || *line == '#';
}

struct conf_entry *conf_parse(FILE *f)
{
  struct conf_entry *head = NULL;
  struct conf_entry *tail = NULL;
  char buf[CONF_MAX_LINE];

  while (fgets(buf, sizeof(buf), f)) {
    struct conf_entry *e;
    char *start = buf;
    char *key;
    char *val;

    strip_newlines(start);
    start = eat_whitespace(start);
    if (is_ignored_line(start))
      continue;

    key = strtok(start, " \t");
    val = strtok(NULL, "");
    if (val)
      val = eat_whitespace(val);
    e = (struct conf_entry *) malloc(sizeof *e);
    if (!e)
      goto fail;
    e->next = NULL;
    e->key = strdup(key);
    e->value = val ? strdup(val) : NULL;
    if (!e->key || (val && !e->value)) {
      free(e->key);
      free(e->value);
      goto fail;
    }
    if (!head) {
      head = e;
      tail = e;
    } else {
      tail->next = e;
      tail = e;
    }
  }

  return head;
fail:
  conf_free(head);
  return NULL;
}

void conf_free(struct conf_entry *e)
{
  struct conf_entry *n;
  while (e) {
    n = e->next;
    free(e->key);
    free(e->value);
    free(e);
    e = n;
  }
}
