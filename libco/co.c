#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>

enum co_status
{
  CO_NEW = 1, // 新创建，还未执行过
  CO_RUNNING, // 已经执行过
  CO_WAITING, // 在 co_wait 上等待
  CO_DEAD,    // 已经结束，但还未释放资源
};

enum setjmp_status
{
  SJ_SAVE = 0,
  SJ_RECOVERY,
};

typedef struct co
{
  char *name;
  void (*func)(void *); // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;     // 协程的状态
  struct co *waiter;         // 是否有其他协程在等待当前协程
  jmp_buf context;           // 寄存器现场 (setjmp.h)
  uint8_t stack[STACK_SIZE]; // 协程的堆栈

  struct co *pre, *next; // 协程链表指针
} Co;

static inline void stack_switch_call(void *sp, void entry(void *), uintptr_t arg);

Co *head = NULL, *tail = NULL, *current = NULL;

static void bl_insert(Co *co)
{
  tail->next = co;
  co->pre = tail;
  co->next = NULL;
  tail = co;
}

static void bl_remove(Co *co)
{
  Co *tco = co;
  if (tco == tail)
  {
    tail = tco->pre;
    tail->next = NULL;
  }
  else
  {
    Co *pco = tco->pre, *nco = tco->next;
    pco->next = nco;
    nco->pre = pco;
  }
  free(tco);
}

static __attribute__((constructor)) void co_init()
{
  head = (Co *)malloc(sizeof(Co));
  Co *main_co = co_start("main", NULL, NULL);
  main_co->status = CO_RUNNING;
  main_co->pre = head;
  head->next = main_co;
  tail = current = main_co;
}

static __attribute__((destructor)) void co_free()
{
  Co *itr = tail;
  while (itr != head)
  {
    bl_remove(itr);
    itr = itr->pre;
  }
  free(head);
}

struct co *co_start(const char *name, void (*func)(void *), void *arg)
{
  Co *co = (Co *)malloc(sizeof(Co));
  memset(co, 0, sizeof(Co));

  co->name = (char *)name;
  co->func = func;
  co->arg = arg;
  co->status = CO_NEW;

  co->next = co->pre = NULL;

  if (head->next != NULL)
    bl_insert(co);

  return co;
}

void co_wait(struct co *co)
{
  co->waiter = current;
  if (co->status != CO_DEAD)
  {
    current->status = CO_WAITING;
    co_yield ();
  }

  assert(co->status == CO_DEAD && co->waiter == current);

  bl_remove(co);
}

void co_yield ()
{
  int ret = 0;
  if ((ret = setjmp(current->context)) == SJ_RECOVERY)
    return;

  while (true)
  {
    current = current->next;
    if (current == NULL)
      current = head->next;

    switch (current->status)
    {
    case CO_NEW:
    {
      current->status = CO_RUNNING;
      stack_switch_call(current->stack + STACK_SIZE, current->func, (uintptr_t)current->arg);
      break;
    }
    case CO_RUNNING:
    {
      longjmp(current->context, SJ_RECOVERY);
      break;
    }
    case CO_DEAD:
    {
      if (current->waiter != NULL)
        current->waiter->status = CO_RUNNING;
      break;
    }
    case CO_WAITING:
    {
      break;
    }
    default:
      assert(false);
    }
  }
}

void co_wrapper(void entry(void *), uintptr_t arg)
{
  entry((void *)arg);

  current->status = CO_DEAD;
  co_yield ();
}

static inline void stack_switch_call(void *sp, void entry(void *), uintptr_t arg)
{
  asm volatile(
#if __x86_64__
      "movq %0, %%rsp; movq %2, %%rdi; jmp %1"
      :
      : "g"((uintptr_t)sp), "d"((void *)entry), "a"((uintptr_t)arg)
      : "memory"
#else
      "movl %0, %%esp; movl %2, %%edi; jmp %1"
      :
      : "g"((uintptr_t)sp)
      : "memory"
#endif
  );
}
