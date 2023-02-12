#ifndef __CO_H__
#define __CO_H__

#define STACK_SIZE (1 << 14) // stack size really effects, I don't know why...... may be related to how compiler uses stack?
                             // if STACK_SIZE is smaller than (1<<14) or bigger than (1 << 16), make test will cause segmentation fault...such as (1 << 13) or (1 << 17)

struct co *co_start(const char *name, void (*func)(void *), void *arg);
void co_yield ();
void co_wait(struct co *co);

#endif