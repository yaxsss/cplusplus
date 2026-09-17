#ifndef CONDITION_H
#define CONDITION_H

#include "../mutex_lock/mutex_lock.h"

#if defined(_WIN32)
#include "condition_windows.h"
#elif defined(__linux__)
#include "condition_linux.h"
#else
#error "Condition: unsupported platform"
#endif

#endif
