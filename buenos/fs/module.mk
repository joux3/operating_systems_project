# Makefile for the drivers module

# Set the module name
MODULE := fs

FILES := vfs.c tfs.c filesystems.c sfs.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))
