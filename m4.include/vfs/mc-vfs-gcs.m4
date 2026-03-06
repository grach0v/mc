dnl GCS (Google Cloud Storage) filesystem support
AC_DEFUN([mc_VFS_GCS],
[
    AC_ARG_ENABLE([vfs-gcs],
		    AS_HELP_STRING([--enable-vfs-gcs], [Support for GCS filesystem @<:@yes@:>@]))
    if test "$enable_vfs" = "yes" -a x"$enable_vfs_gcs" != x"no"; then
	enable_vfs_gcs="yes"
	mc_VFS_ADDNAME([gcs])
	AC_DEFINE([ENABLE_VFS_GCS], [1], [Support for GCS filesystem])
    fi
    AM_CONDITIONAL(ENABLE_VFS_GCS, [test "$enable_vfs" = "yes" -a x"$enable_vfs_gcs" = x"yes"])
])
