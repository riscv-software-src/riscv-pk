// See LICENSE for license details.

#include "emulation.h"
#include "fp_emulation.h"
#include "unprivileged_memory.h"
#include "mtrap.h"
#include "config.h"
#include "pk.h"

#ifdef __riscv_vector

static inline void set_vreg(uintptr_t vlenb, uintptr_t which, uintptr_t pos, uintptr_t size, const uint8_t* bytes)
{
  pos += (which % 8) * vlenb;
  bytes -= pos;

  asm volatile ("vsetvli x0, %0, e8, m8, tu, ma" :: "r" (pos + size));
  write_csr(vstart, pos);

  switch (which / 8) {
    case 0: asm volatile ("vle8.v v0,  (%0)" :: "r" (bytes) : "memory"); break;
    case 1: asm volatile ("vle8.v v8,  (%0)" :: "r" (bytes) : "memory"); break;
    case 2: asm volatile ("vle8.v v16, (%0)" :: "r" (bytes) : "memory"); break;
    case 3: asm volatile ("vle8.v v24, (%0)" :: "r" (bytes) : "memory"); break;
    default:
  }
}

static inline void get_vreg(uintptr_t vlenb, uintptr_t which, uintptr_t pos, uintptr_t size, uint8_t* bytes)
{
  pos += (which % 8) * vlenb;
  bytes -= pos;

  asm volatile ("vsetvli x0, %0, e8, m8, tu, ma" :: "r" (pos + size));
  write_csr(vstart, pos);

  switch (which / 8) {
    case 0: asm volatile ("vse8.v v0,  (%0)" :: "r" (bytes) : "memory"); break;
    case 1: asm volatile ("vse8.v v8,  (%0)" :: "r" (bytes) : "memory"); break;
    case 2: asm volatile ("vse8.v v16, (%0)" :: "r" (bytes) : "memory"); break;
    case 3: asm volatile ("vse8.v v24, (%0)" :: "r" (bytes) : "memory"); break;
    default:
  }
}

static inline void vsetvl(uintptr_t vl, uintptr_t vtype)
{
  asm volatile ("vsetvl x0, %0, %1" :: "r" (vl), "r" (vtype));
}

#define VLEN_MAX 4096

DECLARE_EMULATION_FUNC(misaligned_vec_ldst)
{
  uintptr_t vl = read_csr(vl);
  uintptr_t vtype = read_csr(vtype);
  uintptr_t vlenb = read_csr(vlenb);
  uintptr_t vstart = read_csr(vstart);

  _Bool masked = ((insn >> 25) & 1) == 0;
  _Bool unit = ((insn >> 26) & 3) == 0;
  _Bool strided = ((insn >> 26) & 3) == 2;
  _Bool indexed = !strided && !unit;
  _Bool mew = (insn >> 28) & 1;
  _Bool lumop_simple = ((insn >> 20) & 0x1f) == 0;
  _Bool lumop_whole = ((insn >> 20) & 0x1f) == 8;
  _Bool lumop_fof = ((insn >> 20) & 0x1f) == 16;
  _Bool load = ((insn >> 5) & 1) == 0;
  _Bool illegal = mew || (unit && !(lumop_simple || lumop_whole || (load && lumop_fof)));
  _Bool fof = unit && lumop_fof;
  _Bool whole_reg = unit && lumop_whole;
  uintptr_t vd = (insn >> 7) & 0x1f;
  uintptr_t vs2 = (insn >> 20) & 0x1f;
  uintptr_t vsew = (vtype >> 3) & 3;
  uintptr_t vlmul = vtype & 7;
  uintptr_t view = (insn >> 12) & 3;
  uintptr_t veew = indexed ? vsew : view;
  uintptr_t len = 1 << veew;
  uintptr_t nf0 = 1 + ((insn >> 29) & 7);
  uintptr_t nf = whole_reg ? 1 : nf0;
  uintptr_t evl = whole_reg ? (nf0 * vlenb) >> veew : vl;
  uintptr_t vemul = whole_reg ? 0 : (vlmul + veew - vsew) & 7;
  uintptr_t emul = 1 << ((vemul & 4) ? 0 : vemul);

  uintptr_t base = GET_RS1(insn, regs);
  uintptr_t stride = strided ? GET_RS2(insn, regs) : nf * len;

  if (illegal || vlenb > VLEN_MAX / 8)
    return truly_illegal_insn(regs, mcause, mepc, mstatus, insn);

  uint8_t mask[VLEN_MAX / 8];
  if (masked)
    get_vreg(vlenb, 0, 0, vlenb, mask);

  do {
    if (!masked || ((mask[vstart / 8] >> (vstart % 8)) & 1)) {
      // compute element address
      uintptr_t addr = base + vstart * stride;
      if (indexed) {
        uintptr_t offset = 0;
        get_vreg(vlenb, vs2, vstart << view, 1 << view, (uint8_t *)&offset);
        addr = base + offset;
      }

      uint8_t bytes[8 /* max segments */ * sizeof(uint64_t)];

      if (!load) {
        // obtain store data from regfile
        for (uintptr_t seg = 0; seg < nf; seg++)
          get_vreg(vlenb, vd + seg * emul, vstart * len, len, &bytes[seg * len]);
      }

      // restore clobbered vl/vtype/vstart in case we trap
      vsetvl(vl, vtype);
      write_csr(vstart, vstart);

      if (load) {
        // obtain load data from memory
        for (uintptr_t seg = 0; seg < nf; seg++)
          for (uintptr_t i = 0; i < len; i++)
            bytes[seg * len + i] = load_uint8_t((void *)(addr + seg * len + i), mepc);

        // write load data to regfile
        for (uintptr_t seg = 0; seg < nf; seg++)
          set_vreg(vlenb, vd + seg * emul, vstart * len, len, &bytes[seg * len]);
      } else {
        // write store data to memory
        for (uintptr_t seg = 0; seg < nf; seg++)
          for (uintptr_t i = 0; i < len; i++)
            store_uint8_t((void *)(addr + seg * len + i), bytes[seg * len + i], mepc);
      }
    }
  } while (++vstart < evl && !fof);

  // restore clobbered vl/vtype; vstart=0; advance pc
  vsetvl(fof ? 1 : vl, vtype);
  write_csr(mepc, mepc + 4);
}

#endif
