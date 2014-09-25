/* Integration test helper which will print out
 * the given time in seconds as an unsigned int.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	/* Unsigned int to match what tlsdate -Vraw returns, not time_t */
	unsigned int t = argc > 1 ? (unsigned int) atoi(argv[1]) :
	                 RECENT_COMPILE_DATE + 1;
	fwrite (&t, sizeof (t), 1, stdout);
	fflush(stdout);
	return 0;
}
