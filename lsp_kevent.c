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

static inline lsp_kevent_t * lsp_kevent_construct(lsp_kevent_t * kevent, struct file * file)
{
  *kevent =
    (lsp_kevent_t)
    {
      .file = get_file(file)
      , .p_file = get_task_exe_file(current)
      , .p_cred = get_cred(current->cred)
    };
  return kevent;
}

// ---------------------------------------------------------------------------

static inline void lsp_kevent_destruct(lsp_kevent_t * kevent)
{
  BUG_ON(!kevent);
  if (kevent->file) fput(kevent->file);
  if (kevent->p_file) fput(kevent->p_file);
  if (kevent->p_cred) put_cred(kevent->p_cred);
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

lsp_kevent_t * lsp_kevent_get(struct file * file)
{
  lsp_kevent_t * kevent = kmem_cache_alloc(lsp_kevent_cache, GFP_KERNEL);
  if (unlikely(!kevent))
    return ERR_PTR(-ENOMEM);
  return lsp_keventq_add(lsp_kevent_construct(kevent, file));
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
static lsp_event_field_t __user * lsp_kevent_serialize_field_to_user(const char * from, uint32_t size, uint32_t number, lsp_event_field_t __user * field, uint32_t avail_size)
{
  if (sizeof(number) + sizeof(size) + size <= avail_size)
  {
    size_t err =
      copy_to_user(&field->number, &number, sizeof(number))
      + copy_to_user(&field->size, &size, sizeof(size))
      + copy_to_user(field->value, from, size);
    if (unlikely(err))
    {
      pr_warn("%s: failed to write an event field\n", __func__);
      return ERR_PTR(-EFAULT);
    }
    return lsp_event_field_next(field);
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
  lsp_event_field_t __user * field = NULL;
  lsp_event_t __user * event = NULL;
  char * value = NULL;
  size_t value_size = 0;

  if (unlikely(!kevent || !buffer || !dst))
    return -EINVAL;

  event = (lsp_event_t __user *)dst;
  event->code = kevent->code;
  event->pid = kevent->tgid;
  event->uid = __kuid_val(current->cred->fsuid);
  event->gid = __kgid_val(current->cred->fsgid);
  event->data_size = 0;
  event->field_count = 0;

  // --- filename
  value = d_path(&kevent->file->f_path, buffer, buffer_size);
  if (unlikely(IS_ERR(value)))
    return PTR_ERR(value);

  value_size = strnlen(value, buffer_size) + 1;
  field = lsp_kevent_serialize_field_to_user(value, value_size, 0, lsp_event_field_first(event), avail_size);
  if (unlikely(IS_ERR(field)))
    return PTR_ERR(field);

  event->data_size += value_size;
  event->field_count++;
  avail_size -= value_size;

  // --- issuer
  value = d_path(&kevent->file->f_path, buffer, buffer_size);
  if (unlikely(IS_ERR(value)))
    return PTR_ERR(value);

  value_size = strnlen(value, buffer_size) + 1;
  field = lsp_kevent_serialize_field_to_user(value, value_size, 1, field, avail_size);
  if (unlikely(IS_ERR(field)))
    return PTR_ERR(field);

  event->data_size += value_size;
  event->field_count++;
  avail_size -= value_size;

  return event->data_size;
}

// ---------------------------------------------------------------------------

