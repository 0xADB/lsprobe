#include "lsp_event.h"
#include "lsp_kevent.h"
#include "lsp_listener.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs_struct.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/device.h>

// ---------------------------------------------------------------------------

#define LSP_DEV_NAME "lsprobe"

// ---------------------------------------------------------------------------

static struct class *lsp_class;
static struct device *lsp_device;
static dev_t lsp_dev;
static atomic_t lsp_release = ATOMIC_INIT(0);

// ---------------------------------------------------------------------------

static int lsp_dev_open(struct inode *, struct file *);
static int lsp_dev_release(struct inode *, struct file *);
static ssize_t lsp_dev_read(struct file *, char __user *, size_t, loff_t *);
//static unsigned int lsp_dev_poll(struct file *, poll_table *);
//static long lsp_dev_ioctl(struct file *, unsigned int cmd, unsigned long arg);

static struct file_operations __refdata lsp_dev_ops =
{
  .owner = THIS_MODULE
  ,.open = lsp_dev_open
  ,.release = lsp_dev_release
  ,.read = lsp_dev_read
  //,.poll = lsp_dev_poll
  //,.unlocked_ioctl = lsp_dev_ioctl
  //,.compat_ioctl = lsp_dev_ioctl
};

// ---------------------------------------------------------------------------

static int lsp_dev_open(struct inode *inode, struct file *file)
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

static int lsp_dev_release(struct inode *inode, struct file *file)
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

static ssize_t lsp_dev_read(struct file *file, char __user * dst, size_t avail_size, loff_t *pos)
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

// static unsigned int lsp_poll(struct file *file, poll_table *wait)
// {
//   unsigned int mask;
// 
//   poll_wait(file, &lsp_request_available, wait);
// 
//   mask = POLLOUT | POLLWRNORM;
// 
//   if (!lsp_request_empty())
//     mask |= POLLIN | POLLRDNORM;
// 
//   return mask;
// }

// ---------------------------------------------------------------------------

// static long lsp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
// {
//   struct lsp_proc *proc = lsp_proc_find(current->tgid);
//   if (!proc)
//     return -ENOENT;
// 
//   if ((_IOC_TYPE(cmd) != PRM_LKM_IOC_MAGIC))
//     return -ENOTTY;
// 
//   PDEBUG("%s: cmd: nr: %d\n", __func__, _IOC_NR(cmd));
//   return prm_lkm_ioctl(cmd, (void __user *)arg);
// }

// ---------------------------------------------------------------------------

int lsp_dev_init(void)
{
  int major;

  major = register_chrdev(0, LSP_DEV_NAME, &lsp_dev_ops);
  if (major < 0)
    return major;

  lsp_dev = MKDEV(major, 0);

  lsp_class = class_create(THIS_MODULE, LSP_DEV_NAME);
  if (IS_ERR(lsp_class)) {
    unregister_chrdev(major, LSP_DEV_NAME);
    return PTR_ERR(lsp_class);
  }

  lsp_device = device_create(lsp_class, NULL, lsp_dev, NULL, LSP_DEV_NAME);
  if (IS_ERR(lsp_device)) {
    class_destroy(lsp_class);
    unregister_chrdev(major, LSP_DEV_NAME);
    return PTR_ERR(lsp_device);
  }

  return 0;
}

// ---------------------------------------------------------------------------

void lsp_dev_exit(void)
{
  device_destroy(lsp_class, lsp_dev);
  class_destroy(lsp_class);
  unregister_chrdev(MAJOR(lsp_dev), LSP_DEV_NAME);
}

// ---------------------------------------------------------------------------
