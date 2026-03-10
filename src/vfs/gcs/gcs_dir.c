/* Virtual File System: GCS - directory listing.

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

#include "lib/global.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
gcs_parse_ls_line (struct vfs_class *me, struct vfs_s_inode *dir, struct vfs_s_super *super,
                   const char *line)
{
    struct vfs_s_entry *ent;
    const char *gs_prefix;
    char *url;
    size_t url_len;
    gboolean is_dir = FALSE;
    off_t file_size = 0;
    char *base;
    char *name;

    (void) super;

    if (line[0] == '\0')
        return;

    // skip non-entry lines (TOTAL: summary, gs://...path/: headers)
    {
        const char *trimmed = line;
        while (*trimmed == ' ')
            trimmed++;
        if (g_str_has_prefix (trimmed, "TOTAL:"))
            return;
        if (g_str_has_prefix (trimmed, "gs://") && g_str_has_suffix (trimmed, ":"))
            return;
    }

    // find gs:// URL in the line
    gs_prefix = strstr (line, "gs://");
    if (gs_prefix == NULL)
        return;

    url = g_strstrip (g_strdup (gs_prefix));
    url_len = strlen (url);

    // check if directory (trailing slash)
    if (url_len > 0 && url[url_len - 1] == '/')
    {
        is_dir = TRUE;
        url[url_len - 1] = '\0';
        url_len--;
    }

    // extract basename
    base = strrchr (url, '/');
    if (base != NULL)
        base++;
    else
        base = url;

    if (*base == '\0')
    {
        g_free (url);
        return;
    }

    // skip duplicate entries (gcloud outputs self-reference lines multiple times)
    {
        GList *iter;
        for (iter = g_queue_peek_head_link (dir->subdir); iter != NULL; iter = g_list_next (iter))
        {
            struct vfs_s_entry *existing = (struct vfs_s_entry *) iter->data;
            if (strcmp (existing->name, base) == 0)
            {
                g_free (url);
                return;
            }
        }
    }

    if (!is_dir)
    {
        // parse size from the beginning of the line (before the date and gs:// URL)
        const char *p = line;
        while (*p == ' ')
            p++;
        file_size = (off_t) g_ascii_strtoll (p, NULL, 10);
    }

    name = g_strdup (base);
    g_free (url);

    if (is_dir)
    {
        ent = vfs_s_generate_entry (me, name, dir, S_IFDIR | 0755);
    }
    else
    {
        ent = vfs_s_generate_entry (me, name, dir, S_IFREG | 0644);
        ent->ino->st.st_size = file_size;
    }

    vfs_s_insert_entry (me, dir, ent);
    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
gcs_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, const char *remote_path)
{
    gcs_super_t *gcs_super = GCS_SUPER (dir->super);
    char *gs_url;
    char *cmd;
    char *output = NULL;
    char *error_output = NULL;
    int ret;
    char **lines;
    int i;

    gs_url = gcs_build_gs_url (gcs_super->bucket, remote_path);

    // use -l for file details (size, date)
    cmd = g_strdup_printf ("gcloud storage ls -l '%s'", gs_url);
    g_free (gs_url);

    vfs_print_message ("gcs: Reading %s:%s...", gcs_super->bucket, remote_path);

    ret = gcs_run_command (cmd, &output, &error_output);
    g_free (cmd);

    if (ret != 0)
    {
        me->verrno = EACCES;
        if (error_output != NULL && *error_output != '\0')
            message (D_ERROR, "GCS Error", "%s", error_output);
        else
            message (D_ERROR, "GCS Error", "Failed to list %s:%s", gcs_super->bucket, remote_path);
        g_free (output);
        g_free (error_output);
        return -1;
    }

    if (output != NULL)
    {
        lines = g_strsplit (output, "\n", -1);
        for (i = 0; lines[i] != NULL; i++)
            gcs_parse_ls_line (me, dir, dir->super, lines[i]);
        g_strfreev (lines);
    }

    g_free (output);
    g_free (error_output);

    dir->timestamp = g_get_monotonic_time () + GCS_DIR_CACHE_SECONDS * G_USEC_PER_SEC;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
