# OSAL Memory

## Architecture
- 13 abstract interfaces in src/interface/
- POSIX backend: src/impl/posix/ (uses pthreads)
- CMSIS-OS2 backend: src/impl/cmsis_os/ (maps to FreeRTOS or RT-Thread)
- Backend selected by OSAL_BACKEND_POSIX or OSAL_BACKEND_CMSIS_OS in port's osal_port.h

## Known Fixes
- RWLock.isWriteLocked(): uses writeLocked_ atomic flag (readers also hold writeSemaphore)
- SpinLock.isLocked(): uses atomic<bool> locked_ (osMutexGetOwner deadlocks on POSIX sim)
- Semaphore: constructor max_count=16 for counting semaphore support
- OSALThread stop: cooperative stop via atomic flag + osDelay chunks in cmsis_os backend
