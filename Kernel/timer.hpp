#pragma once

#include <cstdint>

class TimerManager {
  public:
    void Tick();
    unsigned long CurrentTick() const { return tick_; }

  private:
    volatile unsigned long tick_{0};
};

inline TimerManager* timer_manager;

void InitializeAPICTimer();
void StartAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();
void LAPICTimerOnInterrupt();
