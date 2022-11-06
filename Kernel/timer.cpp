#include <cstdint>
#include <deque>

#include "timer.hpp"
#include "acpi.hpp"
#include "interrupt.hpp"
#include "message.hpp"
#include "logger.hpp"
#include "task.hpp"

namespace {
  const uint32_t kCountMax = 0xFFFFFFFFu;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xFEE00320u);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xFEE00380u);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xFEE00390u);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xFEE003E0u);
}

Timer::Timer(unsigned long timeout, int value) : timeout_{timeout}, value_{value} {  
}

TimerManager::TimerManager() {
  timers_.push(Timer{std::numeric_limits<unsigned long>::max(), -1});
}

bool TimerManager::Tick() {
  bool task_timer_timeout = false;

  tick_++;
  while(true) {
    const auto& t = timers_.top();
    if(t.Timeout() > tick_) {
      break;
    }
  
    if(t.Value() == kTaskTimerValue) {
      task_timer_timeout = true;
      timers_.pop();
      timers_.push(Timer{tick_ + kTaskTimerPeriod, kTaskTimerValue});
      break;
    }
  
    Message m{Message::kTimerTimeout};
    m.arg.timer.timeout = t.Timeout();
    m.arg.timer.value = t.Value();        

    task_manager->SendMessage(1, m);

    timers_.pop();
  }

  return task_timer_timeout;
}

void TimerManager::AddTimer(const Timer& timer) {
  timers_.push(timer);
}

void InitializeAPICTimer() {
  timer_manager = new TimerManager();

  divide_config = 0b1011;
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;    

  StartAPICTimer();
  acpi::WaitMilliseconds(1000);
  const auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();

  lapic_timer_freq = static_cast<unsigned long>(elapsed);

  divide_config = 0b1011;
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;
  initial_count = lapic_timer_freq / kTimerFreq;
}

void StartAPICTimer() {
  initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed() {
  return kCountMax - current_count;
}

void StopLAPICTimer() {
  initial_count = 0;
}

void LAPICTimerOnInterrupt() {
  const bool task_timer_timeout = timer_manager->Tick();
  NotifyEndOfInterrupt();

  if(task_timer_timeout && task_manager != nullptr) {
    task_manager->SwitchTask();
  }
}