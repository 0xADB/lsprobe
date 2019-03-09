#ifndef LSP_KEVENT_H
#define LSP_KEVENT_H

// ---------------------------------------------------------------------------

#include "lsp_event.h"

#include <linux/types.h>
#include <linux/cred.h>
#include <linux/file.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/wait.h>

// ---------------------------------------------------------------------------

typedef struct lsp_kevent
{
  struct list_head list_node;
  struct file * file;
  struct file * p_file;
  const struct cred * p_cred;
  lsp_event_code_t code;
  pid_t tgid;
} lsp_kevent_t;

// ---------------------------------------------------------------------------

extern wait_queue_head_t lsp_kevent_available;

// ---------------------------------------------------------------------------

lsp_kevent_t * lsp_kevent_push(struct file *);
void lsp_kevent_put(lsp_kevent_t *);

// ---------------------------------------------------------------------------

bool lsp_keventq_empty(void);
lsp_kevent_t * lsp_keventq_pop(void);
void lsp_keventq_clear(void);
ssize_t lsp_kevent_serialize_to_user(lsp_kevent_t * kevent, char * buffer, size_t buffer_size, char __user * dst, size_t avail_size);

// ---------------------------------------------------------------------------

int lsp_kevent_cache_create(void);
void lsp_kevent_cache_destroy(void);

// ---------------------------------------------------------------------------

#endif // LSP_KEVENT_H
