/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BLEBERRY_BERRY_IO_H_
#define BLEBERRY_BERRY_IO_H_

#include "berry.h"

/* Configure the board I/O (LEDs, buttons) once at startup. */
void berry_io_init(void);

/* Register all native functions (I/O, timing, help) with the VM. */
void berry_io_register(bvm *vm);

#endif /* BLEBERRY_BERRY_IO_H_ */
