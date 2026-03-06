/* Virtual File System: GCS - file operations.

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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/util.h"
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
gcs_build_gs_url_from_vpath (const vfs_path_t *vpath)
{
    const vfs_path_element_t *element;

    element = vfs_path_get_by_index (vpath, -1);
    if (element == NULL || element->host == NULL)
        return NULL;

    return gcs_build_gs_url (element->host, element->path);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
gcs_fh_open (struct vfs_class *me, vfs_file_handler_t *fh, int flags, mode_t mode)
{
    (void) mode;

    if ((flags & O_WRONLY) == O_WRONLY)
    {
        if (fh->ino->localname == NULL)
        {
            vfs_path_t *vpath;
            int handle;

            handle = vfs_mkstemps (&vpath, me->name, fh->ino->ent->name);
            if (handle == -1)
                return (-1);

            close (handle);
            fh->ino->localname = vfs_path_free (vpath, FALSE);
        }
        fh->changed = TRUE;
        return 0;
    }

    if (fh->ino->localname == NULL)
    {
        gcs_super_t *gcs_super = GCS_SUPER (fh->ino->super);
        char *remote_name;
        char *gs_url;
        char *cmd;
        vfs_path_t *tmp_vpath;
        int handle, ret;

        remote_name = vfs_s_fullpath (me, fh->ino);
        if (remote_name == NULL)
        {
            me->verrno = EIO;
            return (-1);
        }

        gs_url = gcs_build_gs_url (gcs_super->bucket, remote_name);
        g_free (remote_name);

        handle = vfs_mkstemps (&tmp_vpath, me->name, fh->ino->ent->name);
        if (handle == -1)
        {
            g_free (gs_url);
            me->verrno = errno;
            return (-1);
        }
        close (handle);
        fh->ino->localname = vfs_path_free (tmp_vpath, FALSE);

        cmd = g_strdup_printf ("gcloud storage cp '%s' '%s'", gs_url, fh->ino->localname);
        vfs_print_message ("gcs: Downloading %s...", gs_url);
        ret = gcs_run_command (cmd, NULL, NULL);
        g_free (cmd);
        g_free (gs_url);

        if (ret != 0)
        {
            unlink (fh->ino->localname);
            MC_PTR_FREE (fh->ino->localname);
            me->verrno = EIO;
            return (-1);
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
gcs_file_store (struct vfs_class *me, vfs_file_handler_t *fh, char *name, char *localname)
{
    gcs_super_t *gcs_super = GCS_SUPER (VFS_FILE_HANDLER_SUPER (fh));
    char *gs_url;
    char *cmd;
    int ret;

    (void) me;

    gs_url = gcs_build_gs_url (gcs_super->bucket, name);
    cmd = g_strdup_printf ("gcloud storage cp '%s' '%s'", localname, gs_url);
    vfs_print_message ("gcs: Uploading %s...", gs_url);
    ret = gcs_run_command (cmd, NULL, NULL);
    g_free (cmd);
    g_free (gs_url);

    return (ret != 0) ? (-1) : 0;
}

/* --------------------------------------------------------------------------------------------- */

int
gcs_cb_unlink (const vfs_path_t *vpath)
{
    char *gs_url;
    char *cmd;
    int ret;

    gs_url = gcs_build_gs_url_from_vpath (vpath);
    if (gs_url == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    cmd = g_strdup_printf ("gcloud storage rm '%s'", gs_url);
    vfs_print_message ("gcs: Deleting %s...", gs_url);
    ret = gcs_run_command (cmd, NULL, NULL);

    g_free (cmd);
    g_free (gs_url);

    if (ret != 0)
    {
        errno = EPERM;
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
gcs_cb_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    char *gs_url1, *gs_url2;
    char *cmd;
    int ret;

    gs_url1 = gcs_build_gs_url_from_vpath (vpath1);
    gs_url2 = gcs_build_gs_url_from_vpath (vpath2);
    if (gs_url1 == NULL || gs_url2 == NULL)
    {
        g_free (gs_url1);
        g_free (gs_url2);
        errno = EINVAL;
        return -1;
    }

    cmd = g_strdup_printf ("gcloud storage mv '%s' '%s'", gs_url1, gs_url2);
    vfs_print_message ("gcs: Moving %s -> %s...", gs_url1, gs_url2);
    ret = gcs_run_command (cmd, NULL, NULL);

    g_free (cmd);
    g_free (gs_url1);
    g_free (gs_url2);

    if (ret != 0)
    {
        errno = EPERM;
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
gcs_cb_mkdir (const vfs_path_t *vpath, mode_t mode)
{
    char *gs_url;
    char *dir_url;
    char *cmd;
    int ret;

    (void) mode;

    gs_url = gcs_build_gs_url_from_vpath (vpath);
    if (gs_url == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (g_str_has_suffix (gs_url, "/"))
        dir_url = g_strdup (gs_url);
    else
        dir_url = g_strconcat (gs_url, "/", NULL);
    g_free (gs_url);

    cmd = g_strdup_printf ("gcloud storage cp /dev/null '%s'", dir_url);
    vfs_print_message ("gcs: Creating directory %s...", dir_url);
    ret = gcs_run_command (cmd, NULL, NULL);

    g_free (cmd);
    g_free (dir_url);

    if (ret != 0)
    {
        errno = EPERM;
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
gcs_cb_rmdir (const vfs_path_t *vpath)
{
    char *gs_url;
    char *dir_url;
    char *cmd;
    int ret;

    gs_url = gcs_build_gs_url_from_vpath (vpath);
    if (gs_url == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (g_str_has_suffix (gs_url, "/"))
        dir_url = g_strdup (gs_url);
    else
        dir_url = g_strconcat (gs_url, "/", NULL);
    g_free (gs_url);

    cmd = g_strdup_printf ("gcloud storage rm -r '%s'", dir_url);
    vfs_print_message ("gcs: Removing directory %s...", dir_url);
    ret = gcs_run_command (cmd, NULL, NULL);

    g_free (cmd);
    g_free (dir_url);

    if (ret != 0)
    {
        errno = EPERM;
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
