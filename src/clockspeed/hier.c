#include "auto_home.h"

void hier()
{
  c("/","etc","leapsecs.dat",-1,-1,0644);

  h(auto_home,-1,-1,0755);

  d(auto_home,"etc",-1,-1,0755);
  d(auto_home,"bin",-1,-1,0755);
  d(auto_home,"man",-1,-1,0755);
  d(auto_home,"man/man1",-1,-1,0755);
  d(auto_home,"man/cat1",-1,-1,0755);

  c(auto_home,"bin","clockspeed",-1,-1,0755);
  c(auto_home,"bin","clockadd",-1,-1,0755);
  c(auto_home,"bin","clockview",-1,-1,0755);
  c(auto_home,"bin","sntpclock",-1,-1,0755);
  c(auto_home,"bin","taiclock",-1,-1,0755);
  c(auto_home,"bin","taiclockd",-1,-1,0755);

  c(auto_home,"man/man1","clockspeed.1",-1,-1,0644);
  c(auto_home,"man/man1","clockadd.1",-1,-1,0644);
  c(auto_home,"man/man1","clockview.1",-1,-1,0644);
  c(auto_home,"man/man1","sntpclock.1",-1,-1,0644);
  c(auto_home,"man/man1","taiclock.1",-1,-1,0644);
  c(auto_home,"man/man1","taiclockd.1",-1,-1,0644);

  c(auto_home,"man/cat1","clockspeed.0",-1,-1,0644);
  c(auto_home,"man/cat1","clockadd.0",-1,-1,0644);
  c(auto_home,"man/cat1","clockview.0",-1,-1,0644);
  c(auto_home,"man/cat1","sntpclock.0",-1,-1,0644);
  c(auto_home,"man/cat1","taiclock.0",-1,-1,0644);
  c(auto_home,"man/cat1","taiclockd.0",-1,-1,0644);
}
