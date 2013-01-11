#include "fmt.h"
#include "scan.h"
#include "ip.h"

unsigned int ip_fmt(s,ip)
char *s;
struct ip_address *ip;
{
  unsigned int len;
  unsigned int i;
 
  len = 0;
  i = fmt_ulong(s,(unsigned long) ip->d[0]); len += i; if (s) s += i;
  i = fmt_str(s,"."); len += i; if (s) s += i;
  i = fmt_ulong(s,(unsigned long) ip->d[1]); len += i; if (s) s += i;
  i = fmt_str(s,"."); len += i; if (s) s += i;
  i = fmt_ulong(s,(unsigned long) ip->d[2]); len += i; if (s) s += i;
  i = fmt_str(s,"."); len += i; if (s) s += i;
  i = fmt_ulong(s,(unsigned long) ip->d[3]); len += i; if (s) s += i;
  return len;
}

unsigned int ip_scan(s,ip)
char *s;
struct ip_address *ip;
{
  unsigned int i;
  unsigned int len;
  unsigned long u;
 
  len = 0;
  i = scan_ulong(s,&u); if (!i) return 0; ip->d[0] = u; s += i; len += i;
  if (*s != '.') return 0; ++s; ++len;
  i = scan_ulong(s,&u); if (!i) return 0; ip->d[1] = u; s += i; len += i;
  if (*s != '.') return 0; ++s; ++len;
  i = scan_ulong(s,&u); if (!i) return 0; ip->d[2] = u; s += i; len += i;
  if (*s != '.') return 0; ++s; ++len;
  i = scan_ulong(s,&u); if (!i) return 0; ip->d[3] = u; s += i; len += i;
  return len;
}

unsigned int ip_scanbracket(s,ip)
char *s;
struct ip_address *ip;
{
  unsigned int len;
 
  if (*s != '[') return 0;
  len = ip_scan(s + 1,ip);
  if (!len) return 0;
  if (s[len + 1] != ']') return 0;
  return len + 2;
}
