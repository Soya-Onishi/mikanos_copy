#pragma once

#include <cstdint>
#include <deque>
#include <queue>

#include "message.hpp"

class Timer {
  public:
    Timer(unsigned long timeout, int value);
    unsigned long Timeout() const { return timeout_; }
    int Value() const { return value_; }

  private:
    unsigned long timeout_;
    int value_;
};

class TimerManager {
  public:
    TimerManager(std::deque<Message>& message_queue);
    void AddTimer(const Timer& timer);
    void Tick();
    unsigned long CurrentTick() const { return tick_; }    

  private:
    volatile unsigned long tick_{0};
    std::priority_queue<Timer> timers_{};
    std::deque<Message>& message_queue_;
};

inline TimerManager* timer_manager;
inline unsigned long lapic_timer_freq = 0;
const int kTimerFreq = 100;

void InitializeAPICTimer(std::deque<Message>& message_queue);
void StartAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();
void LAPICTimerOnInterrupt();

inline bool operator<(const Timer& lhs, const Timer& rhs) {
  return lhs.Timeout() > rhs.Timeout();
}