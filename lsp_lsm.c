
#include "lsp_kevent.h"
#include "lsp_listener.h"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/lsm_hooks.h>

// ----------------------------------------------------------------------------

static bool lsp_gotta_push(struct file * file)
{
  return (
      !(current->flags & PF_KTHREAD)
      && !lsp_listenerq_exists(current->tgid)
      && file != NULL
      && file->f_path.dentry != NULL
      && file->f_path.mnt != NULL
      && S_ISREG(file->f_path.dentry->d_inode->i_mode)
      );
}

// ----------------------------------------------------------------------------

static int lsp_file_open(struct file *file, const struct cred *cred)
{
  if (atomic_read(&lsp_listener_count) && lsp_gotta_push(file))
    lsp_kevent_push(file);
  return 0;
}

// ----------------------------------------------------------------------------

static struct security_hook_list lsp_hooks[] __lsm_ro_after_init =
{
  LSM_HOOK_INIT(file_open, lsp_file_open),
};

// ----------------------------------------------------------------------------

void __init lsprobe_add_hooks(void)
{
  if (lsp_kevent_cache_create() == 0)
  {
    security_add_hooks(lsp_hooks, ARRAY_SIZE(lsp_hooks), "lsprobe");
    pr_info("lsprobe: loaded\n");
  }
}

// ----------------------------------------------------------------------------
