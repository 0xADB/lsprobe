
#include "lsp_kevent.h"
#include "lsp_dev.h"
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
  return !lsp_listenerq_exists(current->tgid);
}

// ----------------------------------------------------------------------------

static int lsp_file_open(struct file *file)
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
  int err = 0;
  err = lsp_dev_init();
  if (unlikely(err))
  {
    pr_err("lsprobe: unable to register device: %d\n", err);
    return;
  }
  security_add_hooks(lsp_hooks, ARRAY_SIZE(lsp_hooks), "lsprobe");
  pr_info("lsprobe: loaded\n");
}

// ---------------------------------------------------------------------------

void __exit lsprobe_exit(void)
{
  lsp_dev_exit();
  pr_info("lsprobe: unloaded\n");
}

// ----------------------------------------------------------------------------
