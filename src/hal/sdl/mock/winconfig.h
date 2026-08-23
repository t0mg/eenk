#pragma once
#include <expat_config.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
static inline bool writeRandomBytes_rand_s(void *target, size_t count) { (void)target; (void)count; return false; }
