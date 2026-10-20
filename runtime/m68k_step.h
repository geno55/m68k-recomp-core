/* Explicit-PC MC68000 execution ABI for generated native instruction steps.
 * No decoder, interpreter, game, OS, host assembly or media is required here.
 * Semantics: Motorola MC68000UM section 6 and M68000PRM.
 */
#ifndef M68K_STEP_H
#define M68K_STEP_H
#include <stddef.h>
#include <stdint.h>

typedef enum { M68K_BUS_OK=1, M68K_BUS_WAIT=2, M68K_BUS_FAULT=3 } M68kStepBusResult;
typedef enum {
    M68K_STEP_OK=0, M68K_STEP_MISS, M68K_STEP_STOPPED,
    M68K_STEP_BUS_ERROR, M68K_STEP_ADDRESS_ERROR, M68K_STEP_NO_WAIT_HANDLER,
    M68K_STEP_REENTERED
} M68kStepResult;
enum { M68K_STEP_AUTOVECTOR=256 };

typedef struct {
    uint32_t D[8], A[8];
    uint16_t SR;
    uint32_t PC, USP;
} M68kStepRegisters;

/* Pure CPU state. No raw-struct portable serialization is promised. */
typedef struct {
    M68kStepRegisters regs;
    /* A7 is the active stack. SSP is its shadow while in user mode; USP is
     * the user-stack shadow while in supervisor mode. */
    uint32_t ssp;
    uint64_t cycles, instructions;
    uint32_t instruction_pc, fault_address;
    uint16_t fault_opcode;
    uint8_t stopped, irq_pins, fault, exception_vector;
} M68kStepState;

typedef struct {
    M68kStepBusResult (*read8)(void *,uint32_t,uint8_t *);
    M68kStepBusResult (*read16)(void *,uint32_t,uint16_t *);
    M68kStepBusResult (*write8)(void *,uint32_t,uint8_t);
    M68kStepBusResult (*write16)(void *,uint32_t,uint16_t);
    /* Side-effect-free instruction bytes from the CURRENT mapped program.
     * WAIT still means physical ownership must be resolved by the scheduler. */
    M68kStepBusResult (*fetch16)(void *,uint32_t,uint16_t *);
    /* Resolve one WAIT by advancing other CPUs/devices; zero means deadlock.
     * The active instruction remains on its own C stack, preserving partial
     * bus effects. Never restart a partially executed instruction on WAIT. */
    int (*wait)(void *,uint32_t,int);
    unsigned (*ack_interrupt)(void *,unsigned); /* vector or AUTOVECTOR */
    void (*reset_devices)(void *);
} M68kStepBus;

typedef struct M68kStepContext {
    M68kStepState state;
    M68kStepBus bus;
    void *user;
    /* Transient execution state. Snapshot only between completed instructions
     * (fault_escape == NULL); these fields are not serialized. */
    void *fault_escape;
    unsigned charge;
} M68kStepContext;

typedef void (*M68kStepOperation)(M68kStepContext *);
typedef struct {
    uint32_t pc;
    uint16_t words[11];
    uint8_t word_count;
    uint16_t cycles;
    M68kStepOperation execute;
} M68kStepEntry;

void m68k_step_init(M68kStepContext *,const M68kStepBus *,void *user);
M68kStepResult m68k_step_reset(M68kStepContext *);
M68kStepResult m68k_step_execute(M68kStepContext *,const M68kStepEntry *);
/* Update physical IRQ pins and accept an eligible autovectored/vectored IRQ.
 * Call only between instructions. Level 7 is edge triggered. */
int m68k_step_interrupt(M68kStepContext *,unsigned level);

uint8_t m68k_step_read8(M68kStepContext *,uint32_t);
uint16_t m68k_step_read16(M68kStepContext *,uint32_t);
uint32_t m68k_step_read32(M68kStepContext *,uint32_t);
void m68k_step_write8(M68kStepContext *,uint32_t,uint8_t);
void m68k_step_write16(M68kStepContext *,uint32_t,uint16_t);
void m68k_step_write32(M68kStepContext *,uint32_t,uint32_t);
void m68k_step_set_sr(M68kStepContext *,uint16_t);
int m68k_step_supervisor(M68kStepContext *);
void m68k_step_exception(M68kStepContext *,unsigned vector,uint32_t return_pc,unsigned total_cycles);
void m68k_step_rte(M68kStepContext *);
void m68k_step_stop(M68kStepContext *,uint16_t sr);
void m68k_step_reset_devices(M68kStepContext *);

static inline uint32_t m68k_step_brief_index_value(uint16_t extension,
                                                  uint32_t dreg,uint32_t areg)
{
    uint32_t value=(extension&0x8000u)?areg:dreg;
    return (extension&0x0800u)?value:(uint32_t)(int32_t)(int16_t)value;
}

#endif
