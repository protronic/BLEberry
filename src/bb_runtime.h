/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BLEBERRY_BB_RUNTIME_H_
#define BLEBERRY_BB_RUNTIME_H_

#include "berry.h"

/* Load the BlockBerry runtime (sps/sensor/escalation/signal/monitor/log)
 * into the VM. Returns 0 on success. */
int bb_runtime_load(bvm *vm);

/* Run due sps.every() tasks. Called from the REPL thread while it is
 * idle waiting for input. Safe to call with NULL. */
void bb_runtime_tick(bvm *vm);

#endif /* BLEBERRY_BB_RUNTIME_H_ */
