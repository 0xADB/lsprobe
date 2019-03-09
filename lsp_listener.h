#ifndef LSP_LISTENER_H
#define LSP_LISTENER_H

#include <linux/types.h>

extern atomic_t lsp_listener_count;

int lsp_listenerq_add(pid_t tgid);
void lsp_listenerq_remove(pid_t tgid);
bool lsp_listenerq_exists(pid_t tgid);

#endif // LSP_LISTENER_H
