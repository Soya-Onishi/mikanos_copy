#include "task.hpp"
#include "timer.hpp"
#include "asmfunc.h"

void InitializeTask() {
  current_task = &task_a_ctx;

  __asm__("cli");
  auto period = timer_manager->CurrentTick() + kTaskTimerPeriod;
  timer_manager->AddTimer(Timer{period, kTaskTimerValue});
  __asm__("sti");
}

void SwitchTask() {
  TaskContext* old_current_task = current_task;
  if(current_task == &task_a_ctx) {
    current_task = &task_b_ctx;    
  } else {
    current_task = &task_a_ctx;
  }

  SwitchContext(current_task, old_current_task);
}