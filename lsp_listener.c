#include "lsp_listener.h"

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/atomic.h>

// ---------------------------------------------------------------------------

typedef struct
{
  struct list_head list_node;
  pid_t tgid;
  atomic_t count;
} lsp_listener_t;

// ---------------------------------------------------------------------------

atomic_t lsp_listener_count = ATOMIC_INIT(0);

// ---------------------------------------------------------------------------

static LIST_HEAD(lsp_listenerq);
static DEFINE_SPINLOCK(lsp_listenerq_lock);

// ---------------------------------------------------------------------------

static lsp_listener_t * lsp_listenerq_find(pid_t tgid)
{
  lsp_listener_t * listener = NULL;
  lsp_listener_t * l;
  spin_lock(&lsp_listenerq_lock);
  list_for_each_entry(l, &lsp_listenerq, list_node)
  {
    if (l->tgid == tgid)
    {
      listener = l;
      break;
    }
  }
  spin_unlock(&lsp_listenerq_lock);
  return listener;
}

// ---------------------------------------------------------------------------

bool lsp_listenerq_exists(pid_t tgid)
{
  return (lsp_listenerq_find(tgid) != NULL);
}

// ---------------------------------------------------------------------------

int lsp_listenerq_add(pid_t tgid)
{
  lsp_listener_t * listener = lsp_listenerq_find(tgid);
  if (listener)
  {
    atomic_inc(&listener->count);
    atomic_inc(&lsp_listener_count);
    return 0;
  }

  listener = kmalloc(sizeof(lsp_listener_t), GFP_KERNEL);
  if (unlikely(!listener))
    return -ENOMEM;

  atomic_set(&listener->count, 1);
  listener->tgid = tgid;

  spin_lock(&lsp_listenerq_lock);
  list_add(&listener->list_node, &lsp_listenerq);
  spin_unlock(&lsp_listenerq_lock);
  atomic_inc(&lsp_listener_count);

  pr_info("lsprobe: added listener: %ld\n", (long)listener->tgid);

  return 0;
}

// ---------------------------------------------------------------------------

void lsp_listenerq_remove(pid_t tgid)
{
  lsp_listener_t * listener = lsp_listenerq_find(tgid);
  if (listener)
  {
    atomic_dec(&lsp_listener_count);
    if (atomic_dec_and_test(&listener->count))
    {
      spin_lock(&lsp_listenerq_lock);
      list_del(&listener->list_node);
      spin_unlock(&lsp_listenerq_lock);
      kfree(listener);
      pr_info("lsprobe: last of %ld left. Farewell, my friend!\n", (long)listener->tgid);
    }
  }
}

// ---------------------------------------------------------------------------
