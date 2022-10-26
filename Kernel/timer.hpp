#pragma once

#include <cstdint>

void InitializeAPICTimer();
void StartAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();
