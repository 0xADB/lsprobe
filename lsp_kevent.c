#include "lsp_kevent.h"

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include <linux/slab.h>
#include <linux/poison.h>

#include <linux/uaccess.h>

// ---------------------------------------------------------------------------

static struct kmem_cache * lsp_kevent_cache = NULL;
static LIST_HEAD(lsp_keventq);
static DEFINE_SPINLOCK(lsp_keventq_lock);

DECLARE_WAIT_QUEUE_HEAD(lsp_kevent_available);

// ---------------------------------------------------------------------------

static inline void lsp_kevent_destruct(lsp_kevent_t * kevent)
{
  BUG_ON(!kevent);
  if (kevent->file) fput(kevent->file);
  if (kevent->p_file) fput(kevent->p_file);
}

// ---------------------------------------------------------------------------

static inline void lsp_kevent_fill_cred(lsp_kevent_t * kevent, const struct task_struct * task)
{
  const struct cred * cred = get_cred(task->cred);
  kevent->p_cred.uid   = __kuid_val(cred->uid);
  kevent->p_cred.gid   = __kgid_val(cred->gid);
  kevent->p_cred.suid  = __kuid_val(cred->suid);
  kevent->p_cred.sgid  = __kgid_val(cred->sgid);
  kevent->p_cred.euid  = __kuid_val(cred->euid);
  kevent->p_cred.egid  = __kgid_val(cred->egid);
  kevent->p_cred.fsuid = __kuid_val(cred->fsuid);
  kevent->p_cred.fsgid = __kgid_val(cred->fsgid);
  kevent->p_cred.tgid  = task->tgid;
  put_cred(cred);
}

static inline lsp_kevent_t * lsp_kevent_construct_file_event(lsp_kevent_t * kevent, lsp_event_code_t code, struct file * file)
{
  INIT_LIST_HEAD(&kevent->list_node);

  kevent->file = get_file(file);

  kevent->p_file = get_task_exe_file(current);
  lsp_kevent_fill_cred(kevent, current);

  kevent->code = code;

  if (unlikely(!kevent->file || !kevent->p_file))
  {
    lsp_kevent_destruct(kevent);
    kevent = NULL;
  }
  return kevent;
}

// ---------------------------------------------------------------------------

static lsp_kevent_t * lsp_keventq_add(lsp_kevent_t * kevent)
{
  spin_lock(&lsp_keventq_lock);
  list_add_tail(&kevent->list_node, &lsp_keventq);
  wake_up_interruptible(&lsp_kevent_available);
  spin_unlock(&lsp_keventq_lock);
  return kevent;
}

// ---------------------------------------------------------------------------

bool lsp_keventq_empty(void)
{
  return list_empty(&lsp_keventq);
}

// ---------------------------------------------------------------------------

lsp_kevent_t * lsp_keventq_pop(void)
{
  lsp_kevent_t * kevent = NULL;
  spin_lock(&lsp_keventq_lock);
  if (!list_empty(&lsp_keventq))
  {
    kevent = list_first_entry(&lsp_keventq, lsp_kevent_t, list_node);
    list_del(&kevent->list_node);
  }
  spin_unlock(&lsp_keventq_lock);
  return kevent;
}

// ---------------------------------------------------------------------------

void lsp_keventq_clear(void)
{
  struct list_head * node;
  struct list_head * next_node;
  lsp_kevent_t * kevent;
  spin_lock(&lsp_keventq_lock);
  list_for_each_safe(node, next_node, &lsp_keventq)
  {
    kevent = list_entry(node, lsp_kevent_t, list_node);
    list_del(node);
    lsp_kevent_destruct(kevent);
    kmem_cache_free(lsp_kevent_cache, kevent);
  }
  spin_unlock(&lsp_keventq_lock);
}

// ---------------------------------------------------------------------------

lsp_kevent_t * lsp_kevent_push(struct file * file)
{
  lsp_kevent_t * kevent = kmem_cache_alloc(lsp_kevent_cache, GFP_KERNEL);
  if (unlikely(!kevent))
    return ERR_PTR(-ENOMEM);

  if (unlikely(!lsp_kevent_construct_file_event(kevent, LSP_EVENT_CODE_FILE_OPEN, file)))
  {
    kmem_cache_free(lsp_kevent_cache, kevent);
    return NULL;
  }
  return lsp_keventq_add(kevent);
}

// ---------------------------------------------------------------------------

void lsp_kevent_put(lsp_kevent_t * kevent)
{
  BUG_ON(!kevent);
  BUG_ON (
      kevent->list_node.next != LIST_POISON1
      || kevent->list_node.prev != LIST_POISON2
      );
  lsp_kevent_destruct(kevent);
  kmem_cache_free(lsp_kevent_cache, kevent);
}

// ---------------------------------------------------------------------------

int lsp_kevent_cache_create(void)
{
  lsp_kevent_cache =
    kmem_cache_create(
	"lsp_kevent_cache"
	, sizeof(lsp_kevent_t)
	, 0
	, SLAB_TEMPORARY
	, NULL
	);
  if (unlikely(!lsp_kevent_cache))
  {
    pr_err("lsp_probe: failed to allocate event cache\n");
    BUG();
    return -EFAULT;
  }
  return 0;
}

// ---------------------------------------------------------------------------

void lsp_kevent_cache_destroy(void)
{
  if (lsp_kevent_cache)
    kmem_cache_destroy(lsp_kevent_cache);
}

// ---------------------------------------------------------------------------

//! copies to user and shifts to the next field
static char __user * lsp_kevent_serialize_field_to_user(const char * from, uint32_t size, uint32_t number, char __user * field, uint32_t avail_size)
{
  BUG_ON(!from);
  BUG_ON(!field);
  if (sizeof(uint32_t) + sizeof(uint32_t) + size <= avail_size)
  {
    size_t err =
      copy_to_user(field, &number, sizeof(uint32_t))
      + copy_to_user(field + sizeof(uint32_t), &size, sizeof(uint32_t))
      + copy_to_user(field + 2 * sizeof(uint32_t), from, size);
    if (unlikely(err))
    {
      pr_warn("%s: failed to write an event field\n", __func__);
      return ERR_PTR(-EFAULT);
    }
    return (field + 2 * sizeof(uint32_t) + size);
  }
  pr_warn("%s: not enough space [%u] to store the value of [%lu] bytes\n"
      , __func__
      , avail_size
      , sizeof(number) + sizeof(size) + size
      );
  return ERR_PTR(-EFAULT);
}

// ---------------------------------------------------------------------------

ssize_t lsp_kevent_serialize_to_user(lsp_kevent_t * kevent, char * buffer, size_t buffer_size, char __user * dst, size_t avail_size)
{
  char __user * field = NULL;
  lsp_event_t __user * event = NULL;
  char * value = NULL;
  size_t value_size = 0;

  BUG_ON(!kevent);
  if (unlikely(!kevent || !buffer || !dst))
    return -EINVAL;

  event = (lsp_event_t __user *)dst;
  event->code = kevent->code;
  event->pcred = kevent->p_cred;
  event->data_size = 0;
  event->field_count = 0;
  field = event->data;

  value = "no_file";
  // --- filename
  if (kevent->file && kevent->file->f_path.mnt && kevent->file->f_path.dentry)
  {
    value = d_path(&kevent->file->f_path, buffer, buffer_size);
    if (unlikely(IS_ERR(value)))
    {
      pr_err("lsprobe: %s: d_path on filename failed: %ld\n", __func__, PTR_ERR(value));
      value = "error";
    }
  }
  else
  {
    pr_err("lsprobe: %s: no file specified\n", __func__);
  }

  value_size = strnlen(value, buffer_size - 1) + 1;

  field = lsp_kevent_serialize_field_to_user(value, value_size, 0, field, avail_size);
  if (unlikely(IS_ERR(field)))
    return PTR_ERR(field);

  event->data_size += value_size;
  event->field_count++;
  avail_size -= value_size;

  value = "no_process";

  if (kevent->p_file && kevent->p_file->f_path.mnt && kevent->p_file->f_path.dentry)
  {
    // --- issuer
    value = d_path(&kevent->p_file->f_path, buffer, buffer_size);
    if (unlikely(IS_ERR(value)))
    {
      pr_err("lsprobe: %s: d_path on process failed: %ld\n", __func__, PTR_ERR(value));
      value = "error";
    }
  }
  else
  {
    pr_err("lsprobe: %s: no process specified\n", __func__);
  }

  value_size = strnlen(value, buffer_size - 1) + 1;

  field = lsp_kevent_serialize_field_to_user(value, value_size, 1, field, avail_size);
  if (unlikely(IS_ERR(field)))
    return PTR_ERR(field);

  event->data_size += value_size;
  event->field_count++;
  avail_size -= value_size;

  pr_info("lsprobe: tgid[%u] real[%u:%u] saved[%u:%u] eff[%u:%u] fs[%u:%u] : [%u] : %s\n"
      , event->pcred.tgid
      , event->pcred.uid
      , event->pcred.gid
      , event->pcred.suid
      , event->pcred.sgid
      , event->pcred.euid
      , event->pcred.egid
      , event->pcred.fsuid
      , event->pcred.fsgid
      , event->code
      , lsp_event_field_first_const(event)->value
      );

  return (sizeof(event) + event->data_size);
}

// ---------------------------------------------------------------------------

