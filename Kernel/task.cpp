#include "task.hpp"
#include "timer.hpp"
#include "segment.hpp"
#include "asmfunc.h"

void InitializeTask() {
  task_manager = new TaskManager;

  __asm__("cli");
  auto period = timer_manager->CurrentTick() + kTaskTimerPeriod;
  timer_manager->AddTimer(Timer{period, kTaskTimerValue});
  __asm__("sti");
}

Task::Task(uint64_t id): id_{id} {  
}

Task& Task::InitContext(TaskFunc* f, int64_t data) {
  const size_t stack_size = kDefaultStackBytes / sizeof(stack_[0]);
  stack_.resize(stack_size);
  uint64_t stack_end = reinterpret_cast<uint64_t>(&stack_[stack_size]);

  memset(&context_, 0, sizeof(context_));
  context_.cr3 = GetCR3();
  context_.rflags = 0x202;
  context_.cs = kKernelCS;
  context_.ss = kKernelSS;
  context_.rsp = (stack_end & ~0xFlu) - 8;
  context_.rip = reinterpret_cast<uint64_t>(f);
  context_.rdi = id_;
  context_.rsi = data;

  *reinterpret_cast<uint32_t*>(&context_.fxsave_area[24]) = 0x1F80;

  return *this;
}

TaskContext& Task::Context() {
  return context_;
}

TaskManager::TaskManager() {
  NewTask();
}

Task& TaskManager::NewTask() {
  latest_id_++;
  return *tasks_.emplace_back(new Task{latest_id_});
}

void TaskManager::SwitchTask() {
  size_t next_task_index = current_task_index_ + 1;
  if(next_task_index >= tasks_.size()) {
    next_task_index = 0;
  }

  if(next_task_index == current_task_index_) {
    return;
  }

  Task& current_task = *tasks_[current_task_index_];
  Task& next_task = *tasks_[next_task_index];
  current_task_index_ = next_task_index;

  SwitchContext(&next_task.Context(), &current_task.Context());
}