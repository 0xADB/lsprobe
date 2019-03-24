#ifndef LSP_LISTENER_H
#define LSP_LISTENER_H

#include <linux/types.h>

int lsp_listenerq_add(pid_t tgid);
void lsp_listenerq_remove(pid_t tgid);
bool lsp_listenerq_exists(pid_t tgid);
bool lsp_listenerq_empty(void);

#endif // LSP_LISTENER_H
