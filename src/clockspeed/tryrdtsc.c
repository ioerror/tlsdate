main()
{
  unsigned long x[2];
  unsigned long y[2];

  x[0] = 0;
  x[1] = 0;
  y[0] = 0;
  y[1] = 0;

  asm volatile(".byte 15;.byte 49" : "=a"(x[0]),"=d"(x[1]) );
  asm volatile(".byte 15;.byte 49" : "=a"(y[0]),"=d"(y[1]) );

  if (x[0] != y[0]) _exit(0);
  if (x[1] != y[1]) _exit(0);
  _exit(1);
}
