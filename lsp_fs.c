#include "lsp_event.h"
#include "lsp_kevent.h"
#include "lsp_listener.h"

#include <linux/printk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/security.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>

// ---------------------------------------------------------------------------

struct lsp_fs
{
  struct dentry * root;
  struct dentry * events;
  struct dentry * tamper;
};

static struct lsp_fs lsp_fs = {.root = NULL, .events = NULL};
static atomic_t lsp_release = ATOMIC_INIT(0);

// ---------------------------------------------------------------------------

static int lsp_fs_events_open(struct inode *, struct file *);
static int lsp_fs_events_release(struct inode *, struct file *);
static ssize_t lsp_fs_events_read(struct file *, char __user *, size_t, loff_t *);

static ssize_t lsp_fs_tamper_write(struct file *, const char __user *, size_t, loff_t *);

// ---------------------------------------------------------------------------

static struct file_operations lsp_fs_events_fops =
{
  .owner = THIS_MODULE
  , .open = lsp_fs_events_open
  , .read = lsp_fs_events_read
  , .release = lsp_fs_events_release
};

static struct file_operations lsp_fs_tamper_fops =
{
  .owner = THIS_MODULE
  , .write = lsp_fs_tamper_write
};

// ---------------------------------------------------------------------------

static int lsp_fs_events_open(struct inode *inode, struct file *file)
{
  if (!file->private_data)
  {
    file->private_data = kmalloc(LSP_EVENT_MAX_SIZE, GFP_KERNEL);
    if (unlikely(!file->private_data))
      return -ENOMEM;
  }
  else
    return -ENOMEM;

  return lsp_listenerq_add(current->tgid);
}

// ---------------------------------------------------------------------------

static int lsp_fs_events_release(struct inode *inode, struct file *file)
{
  lsp_listenerq_remove(current->tgid);
  BUG_ON(file->private_data);
  if (file->private_data)
  {
    kfree(file->private_data);
    file->private_data = NULL;
  }

  if (atomic_dec_and_test(&lsp_listener_count))
  {
    lsp_keventq_clear();
    atomic_set(&lsp_release, 0);
  }
  return 0;
}

// ---------------------------------------------------------------------------

static ssize_t lsp_fs_events_read(struct file *file, char __user * dst, size_t avail_size, loff_t *pos)
{
  lsp_kevent_t * kevent = NULL;
  ssize_t rv = -EINVAL;

  if (unlikely(!file || !dst))
  {
    pr_err("%s: invalid arguments\n", __func__);
    return -EINVAL;
  }
  if (unlikely(!file->private_data))
  {
    pr_err("%s: no listener buffer found\n", __func__);
    return -EINVAL;
  }

  while (lsp_keventq_empty())
  {
    if (unlikely(file->f_flags & O_NONBLOCK))
      return -EAGAIN;
    wait_event_interruptible(lsp_kevent_available, (!lsp_keventq_empty()));
  }

  kevent = lsp_keventq_pop();
  if (unlikely(!kevent))
    return -EAGAIN;

  rv = lsp_kevent_serialize_to_user(kevent, file->private_data, LSP_EVENT_MAX_SIZE, dst, avail_size);

  lsp_kevent_put(kevent);

  return rv;
}

// ---------------------------------------------------------------------------

static ssize_t lsp_fs_tamper_write(struct file *file, const char __user * buf, size_t size, loff_t *pos)
{
  char value = 0;

  if (unlikely(size > sizeof(char)))
    return -EINVAL;
  if (unlikely(copy_from_user(&value, buf, sizeof(char))))
    return -EFAULT;
  atomic_set(&lsp_release, (int)(value == '1'));
  return sizeof(char);
}

// ---------------------------------------------------------------------------

static int __init lsp_create_fs(void)
{
  struct dentry * dentry = NULL;

  if (unlikely(lsp_fs.root))
  {
    pr_err("lsprobe: lsp_fs exists\n");
    return -EEXIST;
  }

  dentry = securityfs_create_dir("lsprobe", NULL);
  if (unlikely(IS_ERR(dentry)))
  {
    pr_err("lsprobe: lsp_fs root error: %ld\n", PTR_ERR(dentry));
    return PTR_ERR(dentry);
  }
  lsp_fs.root = dentry;

  dentry = securityfs_create_file("events", 0400, lsp_fs.root, NULL, &lsp_fs_events_fops);
  if (unlikely(IS_ERR(dentry)))
  {
    pr_err("lsprobe: lsp_fs events error: %ld\n", PTR_ERR(dentry));
    goto error;
  }
  lsp_fs.events = dentry;

  dentry = securityfs_create_file("tamper", 0600, lsp_fs.root, NULL, &lsp_fs_tamper_fops);
  if (unlikely(IS_ERR(dentry)))
  {
    pr_err("lsprobe: lsp_fs tamper error: %ld\n", PTR_ERR(dentry));
    goto error;
  }
  lsp_fs.tamper = dentry;

  return 0;

error:
  if (lsp_fs.events)
    securityfs_remove(lsp_fs.events);
  if (lsp_fs.root)
    securityfs_remove(lsp_fs.root);
  return PTR_ERR(dentry);
}

// ---------------------------------------------------------------------------

fs_initcall(lsp_create_fs);
