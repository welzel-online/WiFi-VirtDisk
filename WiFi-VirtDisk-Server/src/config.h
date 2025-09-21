/* config.h.  Generated from config.h.in by configure.  */
#define CPMTOOLS_VERSION "2.23"

#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_UNISTD_H 1
#if defined(_WIN32)
#define HAVE_WINDOWS_H 1
#define HAVE_WINIOCTL_H 1
#else
#undef HAVE_WINDOWS_H
#undef HAVE_WINIOCTL_H
#define HAVE_DIRENT_H 1
#endif
#define HAVE_LIBDSK_H 1
// #undef  HAVE_LIBDSK_H
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#undef HAVE_SYS_UTIME_H
#define HAVE_UTIME_H 1
#undef HAVE_MODE_T
// Additional
#define HAVE_TIME_H 1
#define HAVE_ERRNO_H 1

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_WINDOWS_H
#include <windows.h>
#endif

#if HAVE_WINIOCTL_H
#include <winioctl.h>
#endif

#if HAVE_LIBDSK_H
#include <libdsk.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_UTIME_H
#include <sys/utime.h>
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#undef NEED_NCURSES
#undef HAVE_NCURSES_NCURSES_H

/* Define either for large file support, if your OS needs them. */
#undef _FILE_OFFSET_BITS
#undef _LARGE_FILES

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX _MAX_PATH
#endif

/* There is not standard header for this, yet it is needed for
 * timezone changes.
 */
#ifndef environ
extern char **environ;
#endif

/* If types are missing, the script defines.awk contained
 * in configure.status should create definitions here.
 */

//#define DISKDEFS	"/usr/local/share/diskdefs"
#define DISKDEFS	"D:/Projekte/libdsk-test/testData/diskdefs"  // neko Java 2023.04.30
#define FORMAT   "ibm-3740"
