# Makefile for the kernel module

# Set the module name
MODULE := kernel_tests


FILES := lock_test.c buffer_test.c sleep_test.c

SRC += $(patsubst %, $(MODULE)/%, $(FILES))

