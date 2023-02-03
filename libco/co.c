#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
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

Co *head = NULL, *tail = NULL, *current = NULL;

static void insert(Co *cor)
{
  tail->next = cor;
  cor->pre = tail;
  cor->next = NULL;
  tail = cor;
}

static void remove(Co *cor)
{
  Co *tcor = cor;
  if (tcor == tail)
  {
    tail = tcor->pre;
    tail->next = NULL;
  }
  else
  {
    Co *pcor = tcor->pre, *ncor = tcor->next;
    pcor->next = ncor;
    ncor->pre = pcor;
  }
  free(tcor);
}

static __attribute__((constructor)) void co_init()
{
  Co *head = (Co *)malloc(sizeof(Co));
  Co *main_cor = co_start("main", NULL, NULL);
  main_cor->pre = head;
  head->next = main_cor;
  tail = current = main_cor;
}

static __attribute__((destructor)) void co_free()
{
  Co *itr = tail;
  while (itr != head)
  {
    remove(itr);
    itr = itr->pre;
  }
}

struct co *co_start(const char *name, void (*func)(void *), void *arg)
{
  Co *cor = (Co *)malloc(sizeof(Co));
  memset(cor, 0, sizeof(Co));

  cor->name = name;
  cor->func = func;
  cor->arg = arg;
  cor->status = CO_NEW;

  cor->next = cor->pre = NULL;

  if (head->next != NULL)
    insert(cor);

  return cor;
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

  remove(co);
}

void co_yield ()
{
  int ret = setjmp(current->context);

  if (ret = SJ_RECOVERY)
    return;

  while (true)
  {
    if (current->next != NULL)
      current = current->next;
    else
      current = head->next;

    switch (current->status)
    {
    case CO_NEW:
    {
      current->status = CO_RUNNING;
      stack_switch_call(current->stack + STACK_SIZE, current->func, current->arg);
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

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg)
{
  asm volatile(
#if __x86_64__
      "movq %0, %%rsp; movq %2, %%rdi; jmp *%1"
      :
      : "b"((uintptr_t)sp), "d"(entry), "a"(arg)
      : "memory"
#else
      "movl %0, %%esp; movl %2, 4(%0); jmp *%1"
      :
      : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg)
      : "memory"
#endif
  );
}
