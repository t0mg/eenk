#pragma once
#ifdef PLATFORM_ESP32

#include "EspEinkDisplay.h"
#include "EspDigitalInput.h"
#include "EspSdmmcStorage.h"
#include <FS.h>
#include <SD_MMC.h>

#ifndef SD_FS
#define SD_FS SD_MMC
#endif

class BatteryMonitor;

using DisplayType = EspEinkDisplay;
using InputType   = EspDigitalInput;
using StorageType = EspSdmmcStorage;

#define HAS_FRONTLIGHT 1
#define HAS_TOUCH 1
#define HAS_FLASH_CACHE 0

#endif // PLATFORM_ESP32
