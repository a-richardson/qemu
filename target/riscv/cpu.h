/*
 * QEMU RISC-V CPU
 *
 * Copyright (c) 2016-2017 Sagar Karandikar, sagark@eecs.berkeley.edu
 * Copyright (c) 2017-2018 SiFive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#include "hw/core/cpu.h"
#include "hw/registerfields.h"
#include "exec/cpu-defs.h"
#include "qemu/units.h"
#include "fpu/softfloat-types.h"
#include "qom/object.h"
#include "cpu_bits.h"
#include "rvfi_dii.h"

#define TCG_GUEST_DEFAULT_MO 0

#define TYPE_RISCV_CPU "riscv-cpu"

#define RISCV_CPU_TYPE_SUFFIX "-" TYPE_RISCV_CPU
#define RISCV_CPU_TYPE_NAME(name) (name RISCV_CPU_TYPE_SUFFIX)
#define CPU_RESOLVING_TYPE TYPE_RISCV_CPU

#define TYPE_RISCV_CPU_ANY              RISCV_CPU_TYPE_NAME("any")
#define TYPE_RISCV_CPU_BASE32           RISCV_CPU_TYPE_NAME("rv32")
#define TYPE_RISCV_CPU_BASE64           RISCV_CPU_TYPE_NAME("rv64")
#define TYPE_RISCV_CPU_IBEX             RISCV_CPU_TYPE_NAME("lowrisc-ibex")
#define TYPE_RISCV_CPU_SHAKTI_C         RISCV_CPU_TYPE_NAME("shakti-c")
#define TYPE_RISCV_CPU_SIFIVE_E31       RISCV_CPU_TYPE_NAME("sifive-e31")
#define TYPE_RISCV_CPU_SIFIVE_E34       RISCV_CPU_TYPE_NAME("sifive-e34")
#define TYPE_RISCV_CPU_SIFIVE_E51       RISCV_CPU_TYPE_NAME("sifive-e51")
#define TYPE_RISCV_CPU_SIFIVE_U34       RISCV_CPU_TYPE_NAME("sifive-u34")
#define TYPE_RISCV_CPU_SIFIVE_U54       RISCV_CPU_TYPE_NAME("sifive-u54")

#if defined(TARGET_RISCV32)
# define TYPE_RISCV_CPU_BASE            TYPE_RISCV_CPU_BASE32
#elif defined(TARGET_RISCV64)
# define TYPE_RISCV_CPU_BASE            TYPE_RISCV_CPU_BASE64
#endif

#define RV(x) ((target_ulong)1 << (x - 'A'))

#define RVI RV('I')
#define RVE RV('E') /* E and I are mutually exclusive */
#define RVM RV('M')
#define RVA RV('A')
#define RVF RV('F')
#define RVD RV('D')
#define RVV RV('V')
#define RVC RV('C')
#define RVS RV('S')
#define RVU RV('U')
#define RVH RV('H')
#define RVJ RV('J')

/* S extension denotes that Supervisor mode exists, however it is possible
   to have a core that support S mode but does not have an MMU and there
   is currently no bit in misa to indicate whether an MMU exists or not
   so a cpu features bitfield is required, likewise for optional PMP support */
enum {
    RISCV_FEATURE_MMU,
    RISCV_FEATURE_PMP,
    RISCV_FEATURE_EPMP,
    RISCV_FEATURE_MISA
};

#define PRIV_VERSION_1_10_0 0x00011000
#define PRIV_VERSION_1_11_0 0x00011100

#define VEXT_VERSION_0_07_1 0x00000701

enum {
    TRANSLATE_SUCCESS,
    TRANSLATE_FAIL,
    TRANSLATE_PMP_FAIL,
    TRANSLATE_G_STAGE_FAIL,
#if defined(TARGET_CHERI) && !defined(TARGET_RISCV32)
    TRANSLATE_CHERI_FAIL,
#endif
};

#define MMU_USER_IDX 3

#define MAX_RISCV_PMPS (16)

typedef struct CPURISCVState CPURISCVState;

#ifdef TARGET_CHERI
#include "cheri-lazy-capregs-types.h"
#endif

#if !defined(CONFIG_USER_ONLY)
#include "pmp.h"
#endif

#define RV_VLEN_MAX 256

FIELD(VTYPE, VLMUL, 0, 2)
FIELD(VTYPE, VSEW, 2, 3)
FIELD(VTYPE, VEDIV, 5, 2)
FIELD(VTYPE, RESERVED, 7, sizeof(target_ulong) * 8 - 9)
FIELD(VTYPE, VILL, sizeof(target_ulong) * 8 - 1, 1)

struct CPURISCVState {
#ifdef TARGET_CHERI
    struct GPCapRegs gpcapregs;
#else
    target_ulong gpr[32];
#endif
    uint64_t fpr[32]; /* assume both F and D extensions */

    /* vector coprocessor state. */
    uint64_t vreg[32 * RV_VLEN_MAX / 64] QEMU_ALIGNED(16);
    target_ulong vxrm;
    target_ulong vxsat;
    target_ulong vl;
    target_ulong vstart;
    target_ulong vtype;

#ifdef TARGET_CHERI
    cap_register_t pcc; // SCR 0 Program counter cap. (PCC) TODO: implement this properly
    cap_register_t ddc; // SCR 1 Default data cap. (DDC)
#else
    target_ulong pc;
#endif
#ifdef CONFIG_DEBUG_TCG
    target_ulong _pc_is_current;
#endif

    target_ulong load_res;
    target_ulong load_val;
#ifdef TARGET_CHERI
    target_ulong load_pesbt;
    bool load_tag;
#endif

    target_ulong frm;

    target_ulong badaddr;
    target_ulong guest_phys_fault_addr;

#ifdef TARGET_CHERI
    // The cause field reports the cause of the last capability exception,
    // following the encoding described in Table 3.9.2.
    // See enum CheriCapExc in cheri-archspecific.h
    uint8_t last_cap_cause; // Used to populate xtval
    // The cap idx field reports the index of the capability register that
    // caused the last exception. When the most significant bit is set, the 5
    // least significant bits are used to index the special purpose capability
    // register file described in Table 5.3, otherwise, they index the
    // general-purpose capability register file.
    uint8_t last_cap_index;  // Used to populate xtval
#endif

#ifdef CONFIG_USER_ONLY
    uint32_t elf_flags;
#endif

#ifndef CONFIG_USER_ONLY
    target_ulong priv;
    /* This contains QEMU specific information about the virt state. */
    target_ulong virt;
    target_ulong resetvec;

    /*
     * For RV32 this is 32-bit mstatus and 32-bit mstatush.
     * For RV64 this is a 64-bit mstatus.
     */
    uint64_t mstatus;

    target_ulong mip;

    uint32_t miclaim;

    target_ulong mie;
    target_ulong mideleg;

    target_ulong satp;   /* since: priv-1.10.0 */
    target_ulong stval;
    target_ulong medeleg;

#if defined(TARGET_CHERI) && !defined(TARGET_RISCV32)
    target_ulong sccsr;
#endif

#ifdef TARGET_CHERI
    cap_register_t jvtc;      // Jump vector table capability
#endif

#ifdef TARGET_CHERI
    cap_register_t stdc;      // SCR 13 Supervisor trap data cap. (STDC)
    cap_register_t sscratchc; // SCR 14 Supervisor scratch cap. (SScratchC)
    cap_register_t sepcc;     // SCR 15 Supervisor exception PC cap. (SEPCC)
    cap_register_t STVECC;
#else
    target_ulong stvec;
    target_ulong sepc;
#endif
    target_ulong scause;

#ifdef TARGET_CHERI
    cap_register_t mtdc;      // SCR 29 Machine trap data cap. (MTDC)
    cap_register_t mscratchc; // SCR 30 Machine scratch cap. (MScratchC)
    cap_register_t mepcc;     // Machine exception PC cap. (MEPCC)
    cap_register_t MTVECC;
#else
    target_ulong mtvec;
    target_ulong mepc;
    target_ulong mscratch;
#endif
    target_ulong mcause;
    target_ulong mtval;  /* since: priv-1.10.0 */

    /* Hypervisor CSRs */
    target_ulong hstatus;
    target_ulong hedeleg;
    target_ulong hideleg;
    target_ulong hcounteren;
    target_ulong htval;
    target_ulong htinst;
    target_ulong hgatp;
    uint64_t htimedelta;

    /* Virtual CSRs */
#ifdef TARGET_CHERI
    cap_register_t vstcc;
    cap_register_t vstdc;
    cap_register_t vsscratchc;
    cap_register_t vsepcc;
#else
    target_ulong vstvec;
    target_ulong vsepc;
#endif
    /*
     * For RV32 this is 32-bit vsstatus and 32-bit vsstatush.
     * For RV64 this is a 64-bit vsstatus.
     */
    uint64_t vsstatus;
    target_ulong vsscratch;
    target_ulong vscause;
    target_ulong vstval;
    target_ulong vsatp;

    target_ulong mtval2;
    target_ulong mtinst;

    /* HS Backup CSRs */
#ifdef TARGET_CHERI
    cap_register_t stcc_hs;
    cap_register_t sepcc_hs;
#else
    target_ulong stvec_hs;
    target_ulong sepc_hs;
#endif
    target_ulong sscratch_hs;
    target_ulong scause_hs;
    target_ulong stval_hs;
    target_ulong satp_hs;
    uint64_t mstatus_hs;

    /* Signals whether the current exception occurred with two-stage address
       translation active. */
    bool two_stage_lookup;

    target_ulong scounteren;
    target_ulong mcounteren;

    target_ulong sscratch;

#ifdef TARGET_CHERI
    cap_register_t dscratch0c;
    cap_register_t dscratch1c;
    cap_register_t dpcc;
    cap_register_t dddc;
#endif
    /* temporary htif regs */
    uint64_t mfromhost;
    uint64_t mtohost;
    uint64_t timecmp;

    /* physical memory protection */
    pmp_table_t pmp_state;
    target_ulong mseccfg;

    /* True if in debugger mode.  */
    bool debugger;

    /*
     * CSRs for PointerMasking extension
     */
    target_ulong mmte;
    target_ulong mpmmask;
    target_ulong mpmbase;
    target_ulong spmmask;
    target_ulong spmbase;
    target_ulong upmmask;
    target_ulong upmbase;
#endif

    float_status fp_status;

#ifdef TARGET_CHERI
    // Some statcounters:
    uint64_t statcounters_cap_read;
    uint64_t statcounters_cap_read_tagged;
    uint64_t statcounters_cap_write;
    uint64_t statcounters_cap_write_tagged;

    uint64_t statcounters_imprecise_setbounds;
    uint64_t statcounters_unrepresentable_caps;

#endif

    /* Fields up to this point are cleared by a TestRIG reset */
    struct {} end_testrig_reset_fields;

    /* machine specific rdtime callback */
    uint64_t (*rdtime_fn)(uint32_t);
    uint32_t rdtime_fn_arg;

#ifdef CONFIG_RVFI_DII
    struct {
        struct rvfi_dii_instruction_metadata INST;
        struct rvfi_dii_pc_data PC;
        struct rvfi_dii_integer_data INTEGER;
        struct rvfi_dii_memory_access_data MEM;
        // TODO: struct rvfi_dii_csr_data CSR;
        // TODO: struct rvfi_dii_fp_data FP;
        // TODO: struct rvfi_dii_cheri_data CHERI;
        // TODO: struct rvfi_dii_cheri_scr_data CHERI_SCR;
        // TODO: struct rvfi_dii_trap_data TRAP;
        uint32_t available_fields;
    } rvfi_dii_trace;
    bool rvfi_dii_have_injected_insn;
    uint32_t rvfi_dii_injected_insn;
#endif

    target_ulong priv_ver;
    target_ulong bext_ver;
    target_ulong vext_ver;
    // TODO: we should probably re-compute these instead of preserving
    //  in case misa becomes writable
    /* RISCVMXL, but uint32_t for vmstate migration */
    uint32_t misa_mxl;      /* current mxl */
    uint32_t misa_mxl_max;  /* max mxl for this cpu */
    uint32_t misa_ext;      /* current extensions */
    uint32_t misa_ext_mask; /* max ext for this cpu */

    target_ulong mhartid;

    target_ulong menvcfg;
    target_ulong senvcfg;
    uint32_t features;

#ifdef CONFIG_USER_ONLY
    uint32_t elf_flags;
#endif

#ifndef CONFIG_USER_ONLY
#endif


    /* Fields from here on are preserved across CPU reset. */
    QEMUTimer *timer; /* Internal timer */
};

static inline bool pc_is_current(CPURISCVState *env)
{
#ifdef CONFIG_DEBUG_TCG
    return env->_pc_is_current;
#else
    return true;
#endif
}

// Note: the pc does not have to be up-to-date, tb start is fine.
// We may miss a few dumps or print too many if -dfilter is on but
// that shouldn't really matter.
static inline target_ulong cpu_get_recent_pc(CPURISCVState *env) {
#ifdef TARGET_CHERI
    return env->pcc._cr_cursor;
#else
    return env->pc;
#endif
}

OBJECT_DECLARE_TYPE(RISCVCPU, RISCVCPUClass,
                    RISCV_CPU)

/**
 * RISCVCPUClass:
 * @parent_realize: The parent class' realize handler.
 * @parent_reset: The parent class' reset handler.
 *
 * A RISCV CPU model.
 */
struct RISCVCPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/
    DeviceRealize parent_realize;
    DeviceReset parent_reset;
};

/**
 * RISCVCPU:
 * @env: #CPURISCVState
 *
 * A RISCV CPU.
 */
struct RISCVCPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/
    CPUNegativeOffsetState neg;
    CPURISCVState env;

    char *dyn_csr_xml;
#ifdef TARGET_CHERI
    char *dyn_scr_xml;
#endif

    /* Configuration Settings */
    struct {
        bool ext_i;
        bool ext_e;
        bool ext_g;
        bool ext_m;
        bool ext_a;
        bool ext_f;
        bool ext_d;
        bool ext_c;
        bool ext_s;
        bool ext_u;
        bool ext_h;
        bool ext_j;
        bool ext_v;
        bool ext_zba;
        bool ext_zbb;
        bool ext_zbc;
        bool ext_zbs;
        bool ext_counters;
        bool ext_ifencei;
        bool ext_icsr;
#ifdef TARGET_CHERI
        bool ext_cheri;
        bool ext_cheri_v9; /* Temporary flag to support new semantics. */
#endif

        char *priv_spec;
        char *user_spec;
        char *bext_spec;
        char *vext_spec;
        uint16_t vlen;
        uint16_t elen;
        bool mmu;
        bool pmp;
        bool epmp;
        uint64_t resetvec;
    } cfg;
};

static inline int riscv_has_ext(CPURISCVState *env, target_ulong ext)
{
    return (env->misa_ext & ext) != 0;
}

static inline bool riscv_feature(CPURISCVState *env, int feature)
{
    return env->features & (1ULL << feature);
}

#include "cpu_user.h"

extern const char * const riscv_int_regnames[];
extern const char * const riscv_fpr_regnames[];
#ifdef TARGET_CHERI
/* Needed for cheri-common logging */
extern const char * const cheri_gp_regnames[];
#endif

#ifdef CONFIG_TCG_LOG_INSTR
void riscv_log_instr_csr_changed(CPURISCVState *env, int csrno);

#ifdef TARGET_CHERI
void riscv_log_instr_scr_changed(CPURISCVState *env, int scrno);
#endif

#define log_changed_special_reg(env, name, newval) do { \
        if (qemu_log_instr_enabled(env))                \
            qemu_log_instr_reg(env, name, newval);      \
    } while(0)
#else /* !CONFIG_TCG_LOG_INSTR */
#define log_changed_special_reg(env, name, newval) ((void)0)
#define riscv_log_instr_csr_changed(env, csrno) ((void)0)
#define riscv_log_instr_scr_changed(env, scrno) ((void)0)
#endif /* !CONFIG_TCG_LOG_INSTR */

/*
 * From 5.3.6 Special Capability Registers (SCRs)
 * Where an SCR extends a RISC-V CSR, e.g. MTCC extending mtvec, any read to the
 * CSR shall return the address (offset for ISAv8) of the corresponding SCR.
 * Similarly, any write to the CSR shall set the address (offset for ISAv8) of
 * the SCR to the value written.
 */
#ifdef TARGET_CHERI
#define SCR_TO_PROGRAM_COUNTER(env, scr)                                       \
    (CHERI_NO_RELOCATION(env) ? cap_get_cursor(scr)                            \
                              : (target_ulong)cap_get_offset(scr))
/**
 * @returns the architectural view of the underlying SCR,address/offset
 * depending on CHERI ISA version.
 */
#define GET_SPECIAL_REG_ARCH(env, name, cheri_name)                            \
    SCR_TO_PROGRAM_COUNTER(env, &((env)->cheri_name))
/**
 * @returns the address of a given SCR as we feed our PC around as an address
 * not the architectural offset.
 */
#define GET_SPECIAL_REG_ADDR(env, name, cheri_name)                            \
    ((target_ulong)cap_get_cursor(&((env)->cheri_name)))
void update_special_register(CPURISCVState *env, cap_register_t *scr,
                             const char *name, target_ulong value);
#define SCR_SET_PROGRAM_COUNTER(env, scr, name, value)                         \
    update_special_register(env, scr, name, value)
#define SET_SPECIAL_REG(env, name, cheri_name, value)                          \
    SCR_SET_PROGRAM_COUNTER(env, &((env)->cheri_name), #cheri_name, value)

#else /* ! TARGET_CHERI */
#define GET_SPECIAL_REG_ARCH(env, name, cheri_name) ((env)->name)
#define GET_SPECIAL_REG_ADDR(env, name, cheri_name) ((env)->name)
#define SET_SPECIAL_REG(env, name, cheri_name, value)                          \
    do {                                                                       \
        env->name = value;                                                     \
        log_changed_special_reg(env, #name, value);                            \
    } while (false)
#endif /* ! TARGET_CHERI */

#ifdef CONFIG_RVFI_DII
#define RVFI_DII_RAM_START 0x80000000
#define RVFI_DII_RAM_SIZE (8 * MiB)
#define RVFI_DII_RAM_END (RVFI_DII_RAM_START + RVFI_DII_RAM_SIZE)
void rvfi_dii_communicate(CPUState *cs, CPURISCVState *env, bool was_trap);
#define CHECK_SAME_TYPE(a, b, msg)                                             \
    _Static_assert(__builtin_types_compatible_p(a*, b*), msg)
#define rvfi_dii_offset(type, field)                                           \
    offsetof(CPURISCVState, rvfi_dii_trace.type.rvfi_##field)
#define gen_rvfi_dii_set_field(type, field, arg)                               \
    do {                                                                       \
        CHECK_SAME_TYPE(                                                       \
            typeof(((CPURISCVState *)NULL)->rvfi_dii_trace.type.rvfi_##field), \
            uint64_t, "Should only be used for uint64_t fields");              \
        CHECK_SAME_TYPE(TCGv_i64, typeof(arg), "Expected 64-bit store");       \
        tcg_gen_st_i64(arg, cpu_env, rvfi_dii_offset(type, field));            \
        tcg_gen_ori_i32(cpu_rvfi_available_fields, cpu_rvfi_available_fields,  \
                        RVFI_##type##_DATA);                                   \
    } while (0)
#define gen_rvfi_dii_set_field_const_iN(n, st_op, type, field, constant)       \
    do {                                                                       \
        CHECK_SAME_TYPE(                                                       \
            typeof(((CPURISCVState *)NULL)->rvfi_dii_trace.type.rvfi_##field), \
            uint##n##_t, "Should only be used for uint64_t fields");           \
        TCGv_i64 rvfi_tc = tcg_const_i64(constant);                            \
        tcg_gen_##st_op(rvfi_tc, cpu_env, rvfi_dii_offset(type, field));       \
        tcg_gen_ori_i32(cpu_rvfi_available_fields, cpu_rvfi_available_fields,  \
                        RVFI_##type##_DATA);                                   \
        tcg_temp_free_i64(rvfi_tc);                                            \
    } while (0)
#define gen_rvfi_dii_set_field_const_i8(type, field, constant)                 \
    gen_rvfi_dii_set_field_const_iN(8, st8_i64, type, field, constant)
#define gen_rvfi_dii_set_field_const_i16(type, field, constant)                \
    gen_rvfi_dii_set_field_const_iN(16, tcg_gen_st16_i64, type, field, constant)
#define gen_rvfi_dii_set_field_const_i32(type, field, constant)                \
    gen_rvfi_dii_set_field_const_iN(32, st32_i64, type, field, constant)
#define gen_rvfi_dii_set_field_const_i64(type, field, constant)                \
    gen_rvfi_dii_set_field_const_iN(64, st_i64, type, field, constant)
#define gen_rvfi_dii_set_field_zext_i32(type, field, arg)                      \
    do {                                                                       \
        CHECK_SAME_TYPE(TCGv_i32, typeof(arg), "Expected i32");                \
        TCGv_i64 tmp = tcg_temp_new_i64();                                     \
        tcg_gen_extu_i32_i64(tmp, arg);                                        \
        gen_rvfi_dii_set_field(type, field, tmp);                              \
        tcg_temp_free_i64(tmp);                                                \
    } while (0)
#if TARGET_LONG_BITS == 32
#define gen_rvfi_dii_set_field_zext_tl(type, field, arg)                       \
    gen_rvfi_dii_set_field_zext_i32(type, field, arg)
#else
#define gen_rvfi_dii_set_field_zext_tl(type, field, arg)                       \
    gen_rvfi_dii_set_field(type, field, arg)
#endif
#define gen_rvfi_dii_set_field_zext_addr(type, field, arg)                     \
    do {                                                                       \
        CHECK_SAME_TYPE(TCGv_cap_checked_ptr, typeof(arg), "Expected addr");   \
        TCGv_i64 tmp = tcg_temp_new_i64();                                     \
        tcg_gen_extu_tl_i64(tmp, (TCGv)arg);                                   \
        gen_rvfi_dii_set_field(type, field, tmp);                              \
        tcg_temp_free_i64(tmp);                                                \
    } while (0)
#define gen_rvfi_dii_set_mem_data(rw, addr, val, memop, extend_to_i64)         \
    do {                                                                       \
        TCGv_i64 tmp = tcg_temp_new_i64();                                     \
        extend_to_i64(tmp, val);                                               \
        tcg_gen_andi_i64(tmp, tmp, MAKE_64BIT_MASK(0, 8 * memop_size(memop))); \
        gen_rvfi_dii_set_field_zext_addr(MEM, mem_addr, addr);                 \
        gen_rvfi_dii_set_field(MEM, mem_##rw##data[0], tmp);                   \
        gen_rvfi_dii_set_field_const_i32(MEM, mem_##rw##mask,                  \
                                         memop_rvfi_mask(memop));              \
        tcg_temp_free_i64(tmp);                                                \
    } while (0)
#define gen_rvfi_dii_set_mem_data_i32(rw, addr, val_i32, memop)                \
    gen_rvfi_dii_set_mem_data(rw, addr, val_i32, memop, tcg_gen_extu_i32_i64)
#define gen_rvfi_dii_set_mem_data_i64(rw, addr, val_i64, memop)                \
    gen_rvfi_dii_set_mem_data(rw, addr, val_i64, memop, tcg_gen_mov_i64)
#else
#define gen_rvfi_dii_set_field(type, field, arg) ((void)0)
#define gen_rvfi_dii_set_field_zext_i32(type, field, arg) ((void)0)
#define gen_rvfi_dii_set_field_zext_addr(type, field, arg) ((void)0)
#define gen_rvfi_dii_set_field_zext_tl(type, field, arg) ((void)0)
#define gen_rvfi_dii_set_field_const_i8(type, field, constant) ((void)0)
#define gen_rvfi_dii_set_field_const_i16(type, field, constant) ((void)0)
#define gen_rvfi_dii_set_field_const_i32(type, field, constant) ((void)0)
#define gen_rvfi_dii_set_field_const_i64(type, field, constant) ((void)0)
#endif

const char *riscv_cpu_get_trap_name(target_ulong cause, bool async);
void riscv_cpu_do_interrupt(CPUState *cpu);
int riscv_cpu_write_elf64_note(WriteCoreDumpFunction f, CPUState *cs,
                               int cpuid, void *opaque);
int riscv_cpu_write_elf32_note(WriteCoreDumpFunction f, CPUState *cs,
                               int cpuid, void *opaque);
int riscv_cpu_gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
int riscv_cpu_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);
bool riscv_cpu_fp_enabled(CPURISCVState *env);
bool riscv_cpu_virt_enabled(CPURISCVState *env);
void riscv_cpu_set_virt_enabled(CPURISCVState *env, bool enable);
bool riscv_cpu_two_stage_lookup(int mmu_idx);
int riscv_cpu_mmu_index(CPURISCVState *env, bool ifetch);
hwaddr riscv_cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
#ifdef TARGET_CHERI
hwaddr cpu_riscv_translate_address_tagmem(CPURISCVState *env,
                                          target_ulong address,
                                          MMUAccessType rw, int reg, int *prot,
                                          uintptr_t retpc);
#endif
void  riscv_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
                                    MMUAccessType access_type, int mmu_idx,
                                    uintptr_t retaddr) QEMU_NORETURN;
bool riscv_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                        MMUAccessType access_type, int mmu_idx,
                        bool probe, uintptr_t retaddr);
void riscv_cpu_do_transaction_failed(CPUState *cs, hwaddr physaddr,
                                     vaddr addr, unsigned size,
                                     MMUAccessType access_type,
                                     int mmu_idx, MemTxAttrs attrs,
                                     MemTxResult response, uintptr_t retaddr);
char *riscv_isa_string(RISCVCPU *cpu);
void riscv_cpu_list(void);

#define cpu_list riscv_cpu_list
#define cpu_mmu_index riscv_cpu_mmu_index

#ifndef CONFIG_USER_ONLY
bool riscv_cpu_exec_interrupt(CPUState *cs, int interrupt_request);
void riscv_cpu_swap_hypervisor_regs(CPURISCVState *env, bool hs_mode_trap);
int riscv_cpu_claim_interrupts(RISCVCPU *cpu, uint32_t interrupts);
uint32_t riscv_cpu_update_mip(RISCVCPU *cpu, uint32_t mask, uint32_t value);
#define BOOL_TO_MASK(x) (-!!(x)) /* helper for riscv_cpu_update_mip value */
void riscv_cpu_set_rdtime_fn(CPURISCVState *env, uint64_t (*fn)(uint32_t),
                             uint32_t arg);
#endif
void riscv_cpu_set_mode(CPURISCVState *env, target_ulong newpriv);

void riscv_translate_init(void);
void QEMU_NORETURN riscv_raise_exception(CPURISCVState *env,
                                         uint32_t exception, uintptr_t pc);

target_ulong riscv_cpu_get_fflags(CPURISCVState *env);
void riscv_cpu_set_fflags(CPURISCVState *env, target_ulong);

#define TB_FLAGS_PRIV_MMU_MASK                3
#define TB_FLAGS_PRIV_HYP_ACCESS_MASK   (1 << 2)
#define TB_FLAGS_MSTATUS_FS MSTATUS_FS

typedef CPURISCVState CPUArchState;
typedef RISCVCPU ArchCPU;
#include "exec/cpu-all.h"
#include "cpu_cheri.h"

FIELD(TB_FLAGS, MEM_IDX, 0, 3)
FIELD(TB_FLAGS, VL_EQ_VLMAX, 3, 1)
FIELD(TB_FLAGS, LMUL, 4, 2)
FIELD(TB_FLAGS, SEW, 6, 3)
FIELD(TB_FLAGS, VILL, 9, 1)
/* Is a Hypervisor instruction load/store allowed? */
FIELD(TB_FLAGS, HLSX, 10, 1)
FIELD(TB_FLAGS, MSTATUS_HS_FS, 11, 2)
/* The combination of MXL/SXL/UXL that applies to the current cpu mode. */
FIELD(TB_FLAGS, XL, 13, 2)
/* If PointerMasking should be applied */
FIELD(TB_FLAGS, PM_ENABLED, 15, 1)

#ifdef TARGET_RISCV32
#define riscv_cpu_mxl(env)  ((void)(env), MXL_RV32)
#else
static inline RISCVMXL riscv_cpu_mxl(CPURISCVState *env)
{
    return env->misa_mxl;
}
#endif

/*
 * A simplification for VLMAX
 * = (1 << LMUL) * VLEN / (8 * (1 << SEW))
 * = (VLEN << LMUL) / (8 << SEW)
 * = (VLEN << LMUL) >> (SEW + 3)
 * = VLEN >> (SEW + 3 - LMUL)
 */
static inline uint32_t vext_get_vlmax(RISCVCPU *cpu, target_ulong vtype)
{
    uint8_t sew, lmul;

    sew = FIELD_EX64(vtype, VTYPE, VSEW);
    lmul = FIELD_EX64(vtype, VTYPE, VLMUL);
    return cpu->cfg.vlen >> (sew + 3 - lmul);
}

void riscv_cpu_get_tb_cpu_state(CPURISCVState *env, target_ulong *pc,
                                target_ulong *cs_base, target_ulong *pcc_base,
                                target_ulong *pcc_top, uint32_t *cheri_flags,
                                uint32_t *pflags);

// Ugly macro hack to avoid having to modify cpu_get_tb_cpu_state in all targets
#define cpu_get_tb_cpu_state_ext riscv_cpu_get_tb_cpu_state

#ifdef CONFIG_TCG_LOG_INSTR
#define RISCV_LOG_INSTR_CPU_U QEMU_LOG_INSTR_CPU_USER
#define RISCV_LOG_INSTR_CPU_S QEMU_LOG_INSTR_CPU_SUPERVISOR
#define RISCV_LOG_INSTR_CPU_H QEMU_LOG_INSTR_CPU_HYPERVISOR
#define RISCV_LOG_INSTR_CPU_M QEMU_LOG_INSTR_CPU_TARGET1
extern const char * const riscv_cpu_mode_names[];

static inline bool cpu_in_user_mode(CPURISCVState *env)
{
    return env->priv == PRV_U;
}

static inline unsigned cpu_get_asid(CPURISCVState *env, target_ulong pc)
{
    return get_field(env->satp, SATP_ASID);
}

static inline const char *cpu_get_mode_name(qemu_log_instr_cpu_mode_t mode)
{
    if (riscv_cpu_mode_names[mode])
        return riscv_cpu_mode_names[mode];
    return "<invalid>";
}
#endif


RISCVException riscv_csrrw(CPURISCVState *env, int csrno, target_ulong *ret_value,
                           target_ulong new_value, target_ulong write_mask,
                           uintptr_t retpc);
RISCVException riscv_csrrw_debug(CPURISCVState *env, int csrno, target_ulong *ret_value,
                                 target_ulong new_value, target_ulong write_mask);

static inline void riscv_csr_write(CPURISCVState *env, int csrno,
                                   target_ulong val, uintptr_t retpc)
{
    riscv_csrrw(env, csrno, NULL, val, MAKE_64BIT_MASK(0, TARGET_LONG_BITS), retpc);
}

static inline target_ulong riscv_csr_read(CPURISCVState *env, int csrno, uintptr_t retpc)
{
    target_ulong val = 0;
    riscv_csrrw(env, csrno, &val, 0, 0, retpc);
    return val;
}

typedef RISCVException (*riscv_csr_predicate_fn)(CPURISCVState *env,
                                                 int csrno);
typedef RISCVException (*riscv_csr_read_fn)(CPURISCVState *env, int csrno,
                                            target_ulong *ret_value);
typedef RISCVException (*riscv_csr_write_fn)(CPURISCVState *env, int csrno,
                                             target_ulong new_value);
typedef RISCVException (*riscv_csr_op_fn)(CPURISCVState *env, int csrno,
                                          target_ulong *ret_value,
                                          target_ulong new_value,
                                          target_ulong write_mask);
typedef void (*riscv_csr_log_update_fn)(CPURISCVState *env, int csrno,
                                        target_ulong new_value);

typedef struct {
    const char *name;
    riscv_csr_predicate_fn predicate;
    riscv_csr_read_fn read;
    riscv_csr_write_fn write;
    riscv_csr_op_fn op;
    riscv_csr_log_update_fn log_update;
} riscv_csr_operations;

/* CSR function table constants */
enum {
    CSR_TABLE_SIZE = 0x1000
};

/* CSR function table */
extern riscv_csr_operations csr_ops[CSR_TABLE_SIZE];

void riscv_get_csr_ops(int csrno, riscv_csr_operations *ops);
void riscv_set_csr_ops(int csrno, riscv_csr_operations *ops);

#ifdef TARGET_CHERI
static inline cap_register_t *riscv_get_scr(CPUArchState *env, uint32_t index)
{
    switch (index) {
    case CheriSCR_PCC: return &env->pcc;
    case CheriSCR_DDC: return &env->ddc;

    case CheriSCR_STDC: return &env->stdc;

    case CheriSCR_MTDC: return &env->mtdc;

    case CheriSCR_BSTCC: return &env->vstcc;
    case CheriSCR_BSTDC: return &env->vstdc;
    case CheriSCR_BSScratchC: return &env->vsscratchc;
    case CheriSCR_BSEPCC: return &env->vsepcc;
    default: assert(false && "Should have raised an invalid inst trap!");
    }
}
#endif

void riscv_cpu_register_gdb_regs_for_features(CPUState *cs);

#ifdef TARGET_CHERI
typedef cap_register_t (*riscv_csr_cap_read_fn)(CPURISCVState *env);
typedef void (*riscv_csr_cap_write_fn)(CPURISCVState *env, cap_register_t* src);

typedef struct {
    const char *name;
    riscv_csr_cap_read_fn read;
    riscv_csr_cap_write_fn write;
}riscv_csr_cap_ops;
riscv_csr_cap_ops* get_csr_cap_info(int csrnum);
#endif


#endif /* RISCV_CPU_H */
