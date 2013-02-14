# ===========================================================================
#       http://
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PLATFORM
#
# DESCRIPTION
#
#   Provide target and host defines.
#
# LICENSE
#
#   Copyright (c) 2012 Brian Aker <brian@tangent.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1
  AC_DEFUN([AX_PLATFORM],
      [AC_REQUIRE([AC_CANONICAL_HOST])
      AC_REQUIRE([AC_CANONICAL_TARGET])

      AC_DEFINE_UNQUOTED([HOST_VENDOR],["$host_vendor"],[Vendor of Build System])
      AC_DEFINE_UNQUOTED([HOST_OS],["$host_os"], [OS of Build System])
      AC_DEFINE_UNQUOTED([HOST_CPU],["$host_cpu"], [CPU of Build System])

      AC_DEFINE_UNQUOTED([TARGET_VENDOR],["$target_vendor"],[Vendor of Target System])
      AC_DEFINE_UNQUOTED([TARGET_OS],["$target_os"], [OS of Target System])
      AC_DEFINE_UNQUOTED([TARGET_CPU],["$target_cpu"], [CPU of Target System])

      AS_CASE([$target_os],
        [*mingw32*],
        [TARGET_WINDOWS="true"
	AC_DEFINE([TARGET_OS_WINDOWS], [1], [Whether we are building for Windows])
        AC_DEFINE([WINVER], [WindowsXP], [Version of Windows])
        AC_DEFINE([_WIN32_WINNT], [0x0501], [Magical number to make things work])
        AC_DEFINE([EAI_SYSTEM], [11], [Another magical number])
        AH_BOTTOM([
#ifndef HAVE_SYS_SOCKET_H
# define SHUT_RD SD_RECEIVE
# define SHUT_WR SD_SEND
# define SHUT_RDWR SD_BOTH
#endif
          ])],
        [*freebsd*],[AC_DEFINE([TARGET_OS_FREEBSD],[1],[Whether we are building for FreeBSD])
        AC_DEFINE([__APPLE_CC__],[1],[Workaround for bug in FreeBSD headers])],
        [*solaris*],[AC_DEFINE([TARGET_OS_SOLARIS],[1],[Whether we are building for Solaris])],
        [*darwin*],
	[TARGET_OSX="true"
	AC_DEFINE([TARGET_OS_OSX],[1],[Whether we build for OSX])],
	[*linux*],
	[TARGET_LINUX="true"
	AC_DEFINE([TARGET_OS_LINUX],[1],[Whether we build for Linux])])

  AM_CONDITIONAL([BUILD_WIN32],[test "x${TARGET_WINDOWS}" = "xtrue"])
  AM_CONDITIONAL([TARGET_OSX],[test "x${TARGET_OSX}" = "xtrue"])
  AM_CONDITIONAL([TARGET_LINUX],[test "x${TARGET_LINUX}" = "xtrue"])
  ])
