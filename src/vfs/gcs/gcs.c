/* Virtual File System: Google Cloud Storage (GCS).
   The interface function

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
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/vfs/utilvfs.h"

#include "internal.h"

/*** global variables ****************************************************************************/

struct vfs_s_subclass gcs_subclass;
struct vfs_class *vfs_gcsfs_ops = VFS_CLASS (&gcs_subclass);

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
gcs_build_gs_url (const char *bucket, const char *path)
{
    if (path == NULL || *path == '\0' || strcmp (path, "/") == 0 || strcmp (path, ".") == 0)
        return g_strdup_printf ("gs://%s", bucket);

    // strip leading "./" if present
    if (path[0] == '.' && path[1] == '/')
        path += 2;

    if (*path == '\0')
        return g_strdup_printf ("gs://%s", bucket);

    if (*path == '/')
        return g_strdup_printf ("gs://%s%s", bucket, path);

    return g_strdup_printf ("gs://%s/%s", bucket, path);
}

/* --------------------------------------------------------------------------------------------- */

int
gcs_run_command (const char *cmd, char **output, char **error_output)
{
    GError *mcerror = NULL;
    mc_pipe_t *p;
    int ret = 0;
    GString *out_str = NULL;
    GString *err_str = NULL;
    GPid child_pid;
    int status = 0;

    if (output != NULL)
        *output = NULL;
    if (error_output != NULL)
        *error_output = NULL;

    p = mc_popen (cmd, TRUE, TRUE, &mcerror);
    if (p == NULL)
    {
        if (mcerror != NULL)
            g_error_free (mcerror);
        return -1;
    }

    child_pid = p->child_pid;

    out_str = g_string_new ("");
    err_str = g_string_new ("");

    while (TRUE)
    {
        p->out.len = MC_PIPE_BUFSIZE;
        p->err.len = MC_PIPE_BUFSIZE;

        mc_pread (p, &mcerror);
        if (mcerror != NULL)
        {
            g_error_free (mcerror);
            mcerror = NULL;
            break;
        }

        if (p->out.len > 0)
            g_string_append_len (out_str, p->out.buf, p->out.len);

        if (p->err.len > 0)
            g_string_append_len (err_str, p->err.buf, p->err.len);

        if (p->out.len == MC_PIPE_STREAM_EOF && p->err.len == MC_PIPE_STREAM_EOF)
            break;

        if (p->out.len == MC_PIPE_ERROR_READ || p->err.len == MC_PIPE_ERROR_READ)
            break;
    }

    // close pipes, reap child
    if (p->out.fd >= 0)
        close (p->out.fd);
    p->out.fd = -1;
    if (p->err.fd >= 0)
        close (p->err.fd);
    p->err.fd = -1;

    // wait for child and check exit status
    {
        int res;
        do
        {
            res = waitpid (child_pid, &status, 0);
        }
        while (res < 0 && errno == EINTR);

        if (res < 0 || !WIFEXITED (status) || WEXITSTATUS (status) != 0)
            ret = -1;
    }

    g_free (p);

    if (output != NULL)
        *output = g_string_free (out_str, FALSE);
    else
        g_string_free (out_str, TRUE);

    if (error_output != NULL)
        *error_output = g_string_free (err_str, FALSE);
    else
        g_string_free (err_str, TRUE);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
gcs_cb_errno (struct vfs_class *me)
{
    (void) me;
    return errno;
}

/* --------------------------------------------------------------------------------------------- */

static void
gcs_cb_fill_names (struct vfs_class *me, fill_names_f func)
{
    GList *iter;

    (void) me;

    for (iter = gcs_subclass.supers; iter != NULL; iter = g_list_next (iter))
    {
        const struct vfs_s_super *super = (const struct vfs_s_super *) iter->data;
        char *name;

        name = g_strdup_printf ("gcs://%s/", GCS_SUPER (super)->bucket);
        func (name);
        g_free (name);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
gcs_archive_same (const vfs_path_element_t *vpath_element, struct vfs_s_super *super,
                  const vfs_path_t *vpath, void *cookie)
{
    (void) vpath;
    (void) cookie;

    return (g_strcmp0 (vpath_element->host, GCS_SUPER (super)->bucket) == 0);
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_super *
gcs_new_archive (struct vfs_class *me)
{
    gcs_super_t *arch;

    arch = g_new0 (gcs_super_t, 1);
    arch->base.me = me;
    arch->base.name = g_strdup (PATH_SEP_STR);

    return VFS_SUPER (arch);
}

/* --------------------------------------------------------------------------------------------- */

static int
gcs_open_archive (struct vfs_s_super *super, const vfs_path_t *vpath,
                  const vfs_path_element_t *vpath_element)
{
    char *cmd;
    char *output = NULL;
    char *error = NULL;
    int ret;

    (void) vpath;

    if (vpath_element->host == NULL || *vpath_element->host == '\0')
    {
        vpath_element->class->verrno = EPERM;
        vfs_print_message ("%s", "gcs: Invalid bucket name.");
        return -1;
    }

    GCS_SUPER (super)->bucket = g_strdup (vpath_element->host);

#ifdef ENABLE_VFS_NET
    super->path_element = vfs_path_element_clone (vpath_element);
#endif

    // validate bucket exists
    cmd = g_strdup_printf ("gcloud storage ls gs://%s", vpath_element->host);
    ret = gcs_run_command (cmd, &output, &error);
    g_free (cmd);

    if (ret != 0)
    {
        char *msg;

        if (error != NULL && *error != '\0')
            msg = g_strdup_printf ("gcs: Cannot access bucket '%s':\n%s",
                                   vpath_element->host, error);
        else
            msg = g_strdup_printf ("gcs: Cannot access bucket '%s'.", vpath_element->host);

        message (D_ERROR, "GCS Error", "%s", msg);
        g_free (msg);
        g_free (output);
        g_free (error);
        vpath_element->class->verrno = EACCES;
        return -1;
    }

    g_free (output);
    g_free (error);

    super->root =
        vfs_s_new_inode (vpath_element->class, super,
                         vfs_s_default_stat (vpath_element->class, S_IFDIR | 0755));

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
gcs_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    (void) me;

    g_free (GCS_SUPER (super)->bucket);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
vfs_init_gcsfs (void)
{
    vfs_init_subclass (&gcs_subclass, "gcsfs",
                       VFSF_NOLINKS | VFSF_REMOTE | VFSF_USETMP, "gcs");

    vfs_gcsfs_ops->fill_names = gcs_cb_fill_names;
    vfs_gcsfs_ops->ferrno = gcs_cb_errno;

    vfs_gcsfs_ops->mkdir = gcs_cb_mkdir;
    vfs_gcsfs_ops->rmdir = gcs_cb_rmdir;
    vfs_gcsfs_ops->unlink = gcs_cb_unlink;
    vfs_gcsfs_ops->rename = gcs_cb_rename;

    gcs_subclass.archive_same = gcs_archive_same;
    gcs_subclass.new_archive = gcs_new_archive;
    gcs_subclass.open_archive = gcs_open_archive;
    gcs_subclass.free_archive = gcs_free_archive;
    gcs_subclass.dir_load = gcs_dir_load;
    gcs_subclass.fh_open = gcs_fh_open;
    gcs_subclass.file_store = gcs_file_store;

    vfs_register_class (vfs_gcsfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
