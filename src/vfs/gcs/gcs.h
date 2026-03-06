/**
 * \file
 * \brief Header: GCS (Google Cloud Storage) VFS
 */

#ifndef MC__VFS_GCS_H
#define MC__VFS_GCS_H

#include "src/filemanager/filegui.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void vfs_init_gcsfs (void);

gboolean gcs_is_gcs_vpath (const vfs_path_t *vpath);
FileProgressStatus gcs_copy_op (const char *src, const char *dst, gboolean is_dir, gboolean rename);
FileProgressStatus gcs_move_op (const char *src, const char *dst, gboolean is_dir, gboolean rename);
FileProgressStatus gcs_delete_op (const char *path, gboolean is_dir);

/*** inline functions ****************************************************************************/

#endif
