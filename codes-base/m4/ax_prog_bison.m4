# ===========================================================================
#          http://www.nongnu.org/autoconf-archive/ax_prog_bison.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PROG_BISON(ACTION-IF-TRUE,ACTION-IF-FALSE)
#
# DESCRIPTION
#
#   Check whether bison is the parser generator. Run ACTION-IF-TRUE if
#   successful, ACTION-IF-FALSE otherwise
#
# LICENSE
#
#   Copyright (c) 2009 Francesco Salvestrini <salvestrini@users.sourceforge.net>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

AC_DEFUN([AX_PROG_BISON], [
  AC_REQUIRE([AC_PROG_YACC])
  AC_REQUIRE([AC_PROG_SED])

  AC_CACHE_CHECK([if bison is the parser generator],[ax_cv_prog_bison],[
    AS_IF([test "`echo \"$YACC\" | $SED 's,^.*\(bison\).*$,\1,'`" = "bison" ],[
      ax_cv_prog_bison=yes
    ],[
      ax_cv_prog_bison=no
    ])
  ])

  AC_DEFINE([HAVE_YACC_OLD_PUSH],[0],[If old-style push parser syntax is supported by ${YACC}])
  AM_CONDITIONAL([HAVE_YACC_OLD_PUSH],[test "x${HAVE_YACC_OLD_PUSH}" == "x1"])
  AC_DEFINE([HAVE_YACC_OLD_PURE],[0],[If old-style pure reentrant parser syntax is supported by ${YACC}])
  AM_CONDITIONAL([HAVE_YACC_OLD_PURE],[test "x${HAVE_YACC_OLD_PURE}" == "x1"])

  AS_IF([test "$ax_cv_prog_bison" = yes],[
    :
    $1
  ],[
    :
    $2
  ])
])
