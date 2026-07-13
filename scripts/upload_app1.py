# upload_app1.py
# Forces PlatformIO to flash this environment to the app1 partition offset

Import("env")

print("Overriding ESP32_APP_OFFSET to 0x710000 for app1")
env.Replace(ESP32_APP_OFFSET="0x710000")
