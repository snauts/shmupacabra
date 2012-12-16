#ifndef GAME2D_CONSOLE_H
#define GAME2D_CONSOLE_H

#include "common.h"

#define CONSOLE_MAX_LINES	5

struct {
	char		buffer[CONSOLE_MAX_LINES][80];
	uint64_t	log_time[CONSOLE_MAX_LINES];
	uint		last_line;
} console;

#endif /* GAME2D_CONSOLE_H */
