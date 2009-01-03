dnl $Id: afs-check.m4,v 1.2.6.1.2.1 2005/08/11 20:14:51 didg Exp $
dnl Autoconf macro to check whether AFS support should be enabled

AC_DEFUN([NETATALK_AFS_CHECK], [
	AFS_LIBS=
	AFS_CFLAGS=

	netatalk_cv_afs=no
	AC_ARG_ENABLE(afs,
		[  --enable-afs            enable AFS support],
		[
			if test "x$enableval" = "xyes"; then
				AC_CHECK_LIB(afsauthent, pioctl, netatalk_cv_afs=yes,
					AC_MSG_ERROR([AFS installation not found])
				)
				AFS_LIBS=-lresolv -lafsrpc -lafsauthent 
				AC_DEFINE(AFS, 1, [Define if AFS should be used])
			fi
		]
	)

	AC_MSG_CHECKING([whether to enable AFS support])	
	if test x"$netatalk_cv_afs" = x"yes"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi

	AC_SUBST(AFS_LIBS)
	AC_SUBST(AFS_CFLAGS)
])
