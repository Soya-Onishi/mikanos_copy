#include <optional>

#include "task.hpp"
#include "timer.hpp"
#include "segment.hpp"
#include "error.hpp"
#include "logger.hpp"
#include "asmfunc.h"

void InitializeTask() {
  task_manager = new TaskManager;

  __asm__("cli");
  auto period = timer_manager->CurrentTick() + kTaskTimerPeriod;
  timer_manager->AddTimer(Timer{period, kTaskTimerValue});
  __asm__("sti");
}

template <class T, class U>
void Erase(T& c, const U& value) {
  auto it = std::remove(c.begin(), c.end(), value);
  c.erase(it, c.end());
}

void TaskIdle(uint64_t task_id, int64_t data) {
  while(true) __asm__("hlt");
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

uint64_t Task::ID() const {
  return id_;
}

bool Task::Running() const {
  return running_;
}

unsigned int Task::Level() const {
  return level_;
}

Task& Task::Sleep() {
  task_manager->Sleep(this);
  return *this;
}

Task& Task::Wakeup() {
  task_manager->Wakeup(this);
  return *this;
}

void Task::SendMessage(const Message& message) {  
  messages_.push_back(message);    
  Wakeup();
}

std::optional<Message> Task::ReceiveMessage() {  
  if(messages_.empty()) {    
    return std::nullopt;
  }  

  auto m = messages_.front();
  messages_.pop_front();
  return m;
}

TaskManager::TaskManager() {
  Task& task = NewTask()
    .SetLevel(current_level_)
    .SetRunning(true);

  running_[current_level_].push_back(&task);

  Task& idle = NewTask()
    .InitContext(TaskIdle, 0)
    .SetLevel(0)
    .SetRunning(true);
  running_[0].push_back(&idle);
}

Task& TaskManager::NewTask() {
  latest_id_++;
  return *tasks_.emplace_back(new Task{latest_id_});
}

Task& TaskManager::CurrentTask() {
  return *running_[current_level_].front();
}

void TaskManager::SwitchTask(bool current_sleep) {
  auto& current_queue = running_[current_level_];
  Task* current_task = current_queue.front();
  current_queue.pop_front();
  if(!current_sleep) {
    current_queue.push_back(current_task);
  }

  if(current_queue.empty()) {
    level_changed_ = true;
  }

  if(level_changed_) {
    level_changed_ = false;
    for(int lv = kMaxLevel; lv >= 0; lv--) {
      if(!running_[lv].empty()) {
        current_level_ = lv;
        break;
      }
    }
  }

  Task* next_task = running_[current_level_].front();
  SwitchContext(&next_task->Context(), &current_task->Context());  
}

Error TaskManager::SendMessage(uint64_t id, const Message& message) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(), [id](const auto& task) { return task->ID() == id; });
  
  if(it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  (*it)->SendMessage(message);
  return MAKE_ERROR(Error::kSuccess);
}

void TaskManager::Sleep(Task* task) {
  if(!task->Running()) {
    return;
  }

  task->SetRunning(false);

  if(task == running_[current_level_].front()) {
    SwitchTask(true);
    return;
  }

  Erase(running_[task->Level()], task);
}

void TaskManager::Wakeup(Task* task, int level) {
  if(task->Running()) {
    ChangeLevelRunning(task, level);
    return;
  }

  if(level < 0) {
    level = task->Level();
  }

  task->SetLevel(level);
  task->SetRunning(true);

  running_[level].push_back(task);
  if(level > current_level_) {
    level_changed_ = true;
  }

  return;
}

void TaskManager::ChangeLevelRunning(Task* task, int level) {
  if(level < 0 || level == task->Level()) {
    return;
  }

  if(task != running_[current_level_].front()) {
    Erase(running_[current_level_], task);
    running_[level].push_back(task);
    task->SetLevel(level);
    if(level > current_level_) {
      level_changed_ = true;
    }    
  } else {
    running_[current_level_].pop_front();
    running_[level].push_front(task);
    task->SetLevel(level);
    if(level >= current_level_) {
      current_level_ = level;
    } else {
      current_level_ = level;
      level_changed_ = true;
    }
  } 
}

Error TaskManager::Sleep(uint64_t id) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(), [id](const auto& task) { return task->ID() == id; });

  if(it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Sleep(it->get());
  return MAKE_ERROR(Error::kSuccess);
}

Error TaskManager::Wakeup(uint64_t id, int level) {
  auto it = std::find_if(tasks_.begin(), tasks_.end(), [id](const auto& task) { return task->ID() == id; });

  if(it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Wakeup(it->get(), level);
  return MAKE_ERROR(Error::kSuccess);
}