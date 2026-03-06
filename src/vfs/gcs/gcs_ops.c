/* Virtual File System: GCS - bulk operations (copy/move/delete).

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/vfs/vfs.h"
#include "lib/vfs/path.h"

#include "src/filemanager/filegui.h"

#include "internal.h"
#include "gcs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Convert an MC VFS path to a gs:// URL for gcloud commands.
 * MC internal paths like "/home/user/gcs://bucket/path" are parsed
 * via the VFS path element to extract bucket and path.
 * Non-GCS paths (local files) are returned as-is.
 */
static char *
gcs_vfs_to_gs_url (const char *mc_path)
{
    vfs_path_t *vpath;
    const vfs_path_element_t *element;
    char *result;

    if (mc_path == NULL)
        return NULL;

    vpath = vfs_path_from_str (mc_path);
    element = vfs_path_get_by_index (vpath, -1);

    if (element == NULL || element->class != vfs_gcsfs_ops || element->host == NULL)
    {
        vfs_path_free (vpath, TRUE);
        return g_strdup (mc_path);
    }

    result = gcs_build_gs_url (element->host, element->path);
    vfs_path_free (vpath, TRUE);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
gcs_is_gcs_vpath (const vfs_path_t *vpath)
{
    const vfs_path_element_t *element;

    if (vpath == NULL)
        return FALSE;

    element = vfs_path_get_by_index (vpath, -1);
    if (element == NULL || element->class == NULL)
        return FALSE;

    return element->class == vfs_gcsfs_ops;
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
gcs_copy_op (const char *src, const char *dst, gboolean is_dir, gboolean rename)
{
    char *gs_src, *gs_dst, *cmd;
    int ret;

    gs_src = gcs_vfs_to_gs_url (src);
    gs_dst = gcs_vfs_to_gs_url (dst);

    /* When renaming a directory, use rsync to copy contents directly
     * into the new name instead of nesting src inside dst */
    if (is_dir && rename)
        cmd = g_strdup_printf ("gcloud storage rsync -r '%s' '%s'", gs_src, gs_dst);
    else if (is_dir)
        cmd = g_strdup_printf ("gcloud storage cp -r '%s' '%s'", gs_src, gs_dst);
    else
        cmd = g_strdup_printf ("gcloud storage cp '%s' '%s'", gs_src, gs_dst);

    ret = gcs_progress_dialog_run (cmd);

    g_free (cmd);
    g_free (gs_src);
    g_free (gs_dst);
    return (FileProgressStatus) ret;
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
gcs_move_op (const char *src, const char *dst, gboolean is_dir, gboolean rename)
{
    char *gs_src, *gs_dst, *cmd;
    int ret;

    gs_src = gcs_vfs_to_gs_url (src);
    gs_dst = gcs_vfs_to_gs_url (dst);

    /* For move+rename, rsync then delete the source */
    if (is_dir && rename)
        cmd = g_strdup_printf ("gcloud storage rsync -r '%s' '%s' && gcloud storage rm -r '%s'",
                               gs_src, gs_dst, gs_src);
    else if (is_dir)
        cmd = g_strdup_printf ("gcloud storage mv -r '%s' '%s'", gs_src, gs_dst);
    else
        cmd = g_strdup_printf ("gcloud storage mv '%s' '%s'", gs_src, gs_dst);

    ret = gcs_progress_dialog_run (cmd);

    g_free (cmd);
    g_free (gs_src);
    g_free (gs_dst);
    return (FileProgressStatus) ret;
}

/* --------------------------------------------------------------------------------------------- */

FileProgressStatus
gcs_delete_op (const char *path, gboolean is_dir)
{
    char *gs_path, *cmd;
    int ret;

    gs_path = gcs_vfs_to_gs_url (path);

    if (is_dir)
        cmd = g_strdup_printf ("gcloud storage rm -r '%s'", gs_path);
    else
        cmd = g_strdup_printf ("gcloud storage rm '%s'", gs_path);

    ret = gcs_progress_dialog_run (cmd);

    g_free (cmd);
    g_free (gs_path);
    return (FileProgressStatus) ret;
}

/* --------------------------------------------------------------------------------------------- */
