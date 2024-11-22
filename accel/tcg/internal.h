/*
 * Internal execution defines for qemu
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef ACCEL_TCG_INTERNAL_H
#define ACCEL_TCG_INTERNAL_H

#include "exec/exec-all.h"

TranslationBlock *tb_gen_code(CPUState *cpu, target_ulong pc,
                              target_ulong cs_base, target_ulong pcc_base,
                              target_ulong pcc_top, uint32_t cheri_flags,
                              uint32_t flags, int cflags);

void QEMU_NORETURN cpu_io_recompile(CPUState *cpu, uintptr_t retaddr);

#endif /* ACCEL_TCG_INTERNAL_H */
