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
    TimerManager();
    void AddTimer(const Timer& timer);
    bool Tick();
    unsigned long CurrentTick() const { return tick_; }    

  private:
    volatile unsigned long tick_{0};
    std::priority_queue<Timer> timers_{};    
};

inline TimerManager* timer_manager;
inline unsigned long lapic_timer_freq = 0;
const int kTimerFreq = 100;

const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 0.02);
const int kTaskTimerValue = std::numeric_limits<int>::min();

void InitializeAPICTimer();
void StartAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();
void LAPICTimerOnInterrupt();

inline bool operator<(const Timer& lhs, const Timer& rhs) {
  return lhs.Timeout() > rhs.Timeout();
}