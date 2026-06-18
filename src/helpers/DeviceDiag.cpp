#include "DeviceDiag.h"

#if defined(NRF52_PLATFORM)
#include "FreeRTOS.h"
#include "task.h"

// Same linker symbols + _sbrk() the core's own new.cpp uses to implement
// malloc's heap (see framework-arduinoadafruitnrf52/cores/nRF5/new.cpp) —
// there's no separate "free heap" API on this core, so this mirrors it.
extern "C" char* _sbrk(int incr);
extern unsigned char __HeapBase[];
extern unsigned char __HeapLimit[];

void DeviceDiag::getHeapStats(uint32_t& free_bytes, uint32_t& total_bytes) {
  total_bytes = (uint32_t)(__HeapLimit - __HeapBase);
  uint32_t used = (uint32_t)((unsigned char*)_sbrk(0) - __HeapBase);
  free_bytes = (used < total_bytes) ? (total_bytes - used) : 0;
}

uint32_t DeviceDiag::getStackFreeBytes() {
  return (uint32_t)uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
}

#else

void DeviceDiag::getHeapStats(uint32_t& free_bytes, uint32_t& total_bytes) {
  free_bytes = 0; total_bytes = 0;
}
uint32_t DeviceDiag::getStackFreeBytes() { return 0; }

#endif
