# $Header$
# This comes from the official Autoconf macro archive at
# <http://research.cys.de/autoconf-archive/>
# (I removed the $ before the Id CVS keyword below.)


dnl @synopsis AC_FUNC_ACCEPT_ARGTYPES
dnl
dnl Checks the data types of the three arguments to accept(). Results are
dnl placed into the symbols ACCEPT_TYPE_ARG[123], consistent with the
dnl following example:
dnl
dnl       #define ACCEPT_TYPE_ARG1 int
dnl       #define ACCEPT_TYPE_ARG2 struct sockaddr *
dnl       #define ACCEPT_TYPE_ARG3 socklen_t
dnl
dnl This macro requires AC_CHECK_HEADERS to have already verified the
dnl presence or absence of sys/types.h and sys/socket.h.
dnl
dnl NOTE: This is just a modified version of the AC_FUNC_SELECT_ARGTYPES
dnl macro. Credit for that one goes to David MacKenzie et. al.
dnl
dnl @version Id: ac_func_accept_argtypes.m4,v 1.1 1999/12/03 11:29:29 simons Exp $
dnl @author Daniel Richard G. <skunk@mit.edu>
dnl

# PostgreSQL local changes: In the original version ACCEPT_TYPE_ARG3
# is a pointer type. That's kind of useless because then you can't
# use the macro to define a corresponding variable. We also make the
# reasonable(?) assumption that you can use arg3 for getsocktype etc.
# as well (i.e., anywhere POSIX.2 has socklen_t).

AC_DEFUN(AC_FUNC_ACCEPT_ARGTYPES,
[AC_MSG_CHECKING([types of arguments for accept()])
 AC_CACHE_VAL(ac_cv_func_accept_arg1,dnl
 [AC_CACHE_VAL(ac_cv_func_accept_arg2,dnl
  [AC_CACHE_VAL(ac_cv_func_accept_arg3,dnl
   [for ac_cv_func_accept_arg1 in 'int' 'unsigned int'; do
     for ac_cv_func_accept_arg2 in 'struct sockaddr *' 'void *'; do
      for ac_cv_func_accept_arg3 in 'socklen_t' 'size_t' 'unsigned int' 'int'; do
       AC_TRY_COMPILE(dnl
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
extern accept ($ac_cv_func_accept_arg1, $ac_cv_func_accept_arg2, $ac_cv_func_accept_arg3 *);],,dnl
        [ac_not_found=no ; break 3], ac_not_found=yes)
      done
     done
    done
   ])dnl AC_CACHE_VAL
  ])dnl AC_CACHE_VAL
 ])dnl AC_CACHE_VAL
 if test "$ac_not_found" = yes; then
  ac_cv_func_accept_arg1=int
  ac_cv_func_accept_arg2='struct sockaddr *'
  ac_cv_func_accept_arg3='socklen_t'
 fi
 AC_MSG_RESULT([$ac_cv_func_accept_arg1, $ac_cv_func_accept_arg2, $ac_cv_func_accept_arg3 *])
 AC_DEFINE_UNQUOTED(ACCEPT_TYPE_ARG1,$ac_cv_func_accept_arg1)
 AC_DEFINE_UNQUOTED(ACCEPT_TYPE_ARG2,$ac_cv_func_accept_arg2)
 AC_DEFINE_UNQUOTED(ACCEPT_TYPE_ARG3,$ac_cv_func_accept_arg3)
])
