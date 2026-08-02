#pragma once
#ifdef PLATFORM_ESP32

#include "EspEinkDisplay.h"
#include "EspAdcInput.h"
#include "EspSdStorage.h"
#include <FS.h>
#include <SD.h>

#ifndef SD_FS
#define SD_FS SD
#endif

class BatteryMonitor;

using DisplayType = EspEinkDisplay;
using InputType   = EspAdcInput;
using StorageType = EspSdStorage;

#define HAS_FRONTLIGHT 0
#define HAS_TOUCH 0
#define HAS_FLASH_CACHE 1

#endif // PLATFORM_ESP32
