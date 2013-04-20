Please feel free to contribute patches; here are the basic guidelines to hack
along with us!

Please work from a git tree by cloning the repo:

  git clone https://github.com/ioerror/tlsdate.git

Please file bugs on the tlsdate issue tracker:

  https://github.com/ioerror/tlsdate/issues

Please use the github pull request feature when possible.

The current build status is available as a handy image:

[![Build Status](https://secure.travis-ci.org/ioerror/tlsdate.png?branch=master)](http://travis-ci.org/ioerror/tlsdate)

Continuous integration is available for a number of platforms:

  https://jenkins.torproject.org/job/tlsdate-ci-linux/
  https://travis-ci.org/ioerror/tlsdate
  http://build.chromium.org/p/chromiumos/waterfall

White Space:

  Spaces only, no tabs; all tabs must die
  No stray spaces at the end of lines
  Generally try not to add excessive empty white space

Documentation:

  Document all functions with doxygen style comments

Ensuring Correctness:

  Test your patches and ensure:

    No compiler warnings or errors
    No linker warnings or errors

  Test your improved copy of tlsdate extensively

Security:

  tlsdate is security sensitive - please consider where you add code and in
  what context it will run. When possible, run with the least privilege as is
  possible.

Proactively find bugs:

 Run your copy of tlsdate under valgrind

Weird but meaningful conventions are prefered in tlsdate. We prefer attention
to detail:

  if ( NULL == foo (void) )
  {
    bar (void);
  }

Over quick, hard to read and potentilly incorrect:

  if (foo(void)==NULL))
    bar();

Define magic numbers and explain their origin:

  // As taken from RFC 3.14
  #define MAGIC_NUMBER 23 // This goes in foo.h
  ptr = malloc (MAGIC_NUMBER);

Rather than just throwing them about in code:

  ptr = malloc (23);

It is almost always prefered to use dynamically allocated memory:

  widget_ptr = malloc (WIDGET_SIZE);

Try to avoid static allocations like the following:

  char widget[WIDGET_SIZE];

Try to use unsigned values unless an API requires signed values:

  uint32_t server_time_s;

Please provide relevant CHANGELOG entries for all changes.
Please remove items from the TODO file as they are completed.
Please provide unittest cases.

When submitting patches via email, please use `git format-patch` to format
patches:

  git format-patch 9a61fcba9bebc3fa2d91c9f79306bf316c59cbcc

Email patches with a GnuPG signature whenever possible.

When applying patches, please use `git am` to apply patches:

  git am -i 0001-add-TODO-item.patch

If `git format-patch` is not possible, please send a unified diff.

When in doubt, please consult the Tor HACKING guide:

  https://gitweb.torproject.org/tor.git/blob/HEAD:/doc/HACKING

