/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

int main(void)
{
	while (1) {
		printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
