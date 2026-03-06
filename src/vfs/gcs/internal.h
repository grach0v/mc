/**
 * \file
 * \brief Header: GCS VFS internals
 */

#ifndef MC__VFS_GCS_INTERNAL_H
#define MC__VFS_GCS_INTERNAL_H

#include "lib/vfs/vfs.h"
#include "lib/vfs/xdirentry.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define GCS_SUPER(super) ((gcs_super_t *) (super))

#define GCS_DIR_CACHE_SECONDS 30

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    struct vfs_s_super base;
    char *bucket;
} gcs_super_t;

/*** global variables defined in .c file *********************************************************/

extern struct vfs_s_subclass gcs_subclass;
extern struct vfs_class *vfs_gcsfs_ops;

/*** declarations of public functions ************************************************************/

/* gcs.c */
char *gcs_build_gs_url (const char *bucket, const char *path);
int gcs_run_command (const char *cmd, char **output, char **error_output);

/* gcs_dir.c */
int gcs_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, const char *remote_path);

/* gcs_file.c */
int gcs_fh_open (struct vfs_class *me, vfs_file_handler_t *fh, int flags, mode_t mode);
int gcs_file_store (struct vfs_class *me, vfs_file_handler_t *fh, char *name, char *localname);
int gcs_cb_unlink (const vfs_path_t *vpath);
int gcs_cb_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2);
int gcs_cb_mkdir (const vfs_path_t *vpath, mode_t mode);
int gcs_cb_rmdir (const vfs_path_t *vpath);

/* gcs_progress_ui.c */
int gcs_progress_dialog_run (const char *cmd);

/*** inline functions ****************************************************************************/

#endif
