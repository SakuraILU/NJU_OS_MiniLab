#ifndef __CO_H__
#define __CO_H__

#define STACK_SIZE (1 << 19) // this effects... I don't know why..if STACK_SIZE is set to (1<<13) or even smaller, make test will cause segment fault...
#define NCOR 64

struct co *co_start(const char *name, void (*func)(void *), void *arg);
void co_yield ();
void co_wait(struct co *co);

#endif