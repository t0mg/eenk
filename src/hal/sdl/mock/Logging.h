#pragma once
#include <cstdio>
#define LOG_E(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define LOG_ERR(tag, fmt, ...) printf("[%s] ERROR: " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_INF(tag, fmt, ...) printf("[%s] INFO: " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_DBG(tag, fmt, ...) printf("[%s] DBG: " fmt "\n", tag, ##__VA_ARGS__)
