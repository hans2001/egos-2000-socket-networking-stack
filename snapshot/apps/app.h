#pragma once

#include "egos.h"
#include "syscall.h"
#include "uip/uip.h"
#include "uip/uipopt.h"
#include "uip/uip_arp.h"

struct grass* grass = (void*)GRASS_STRUCT;
struct earth* earth = (void*)EARTH_STRUCT;

#define workdir_ino (*(int*)(SHELL_WORK_DIR))
#define workdir     ((char*)(SHELL_WORK_DIR + sizeof(int)))
