obj-$(CONFIG_SECURITY_LSPROBE) := lsprobe.o

lsprobe-y := lsp_lsm.o lsp_kevent.o lsp_listener.o lsp_fs.o
