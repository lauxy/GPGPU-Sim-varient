// Copyright (c) 2009-2011, Tor M. Aamodt, Ali Bakhoda
// The University of British Columbia
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution. Neither the name of
// The University of British Columbia nor the names of its contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "ptx_sim.h"
#include <string>
#include "ptx_ir.h"
class ptx_recognizer;
typedef void *yyscan_t;
#include "../../libcuda/gpgpu_context.h"
#include "../gpgpu-sim/gpu-sim.h"
#include "../gpgpu-sim/shader.h"
#include "ptx.tab.h"
#include "ptx_compiler.h"

void feature_not_implemented(const char *f);

ptx_cta_info::ptx_cta_info(unsigned sm_idx, gpgpu_context *ctx) {
  assert(ctx->func_sim->g_ptx_cta_info_sm_idx_used.find(sm_idx) ==
         ctx->func_sim->g_ptx_cta_info_sm_idx_used.end());
  ctx->func_sim->g_ptx_cta_info_sm_idx_used.insert(sm_idx);

  m_sm_idx = sm_idx;
  m_uid = (ctx->g_ptx_cta_info_uid)++;
  m_bar_threads = 0;
  gpgpu_ctx = ctx;
}

void ptx_cta_info::print_ptx_cta_info() {
  printf("DEBUG INFO: <cta info> sm_idx: %d, uid: %d, threads_per_cta: %d\n", m_sm_idx, m_uid, m_threads_in_cta.size());
}

void ptx_cta_info::add_thread(ptx_thread_info *thd) {
  m_threads_in_cta.insert(thd);
}

unsigned ptx_cta_info::num_threads() const { return m_threads_in_cta.size(); }

void ptx_cta_info::check_cta_thread_status_and_reset() {
  bool fail = false;
  if (m_threads_that_have_exited.size() != m_threads_in_cta.size()) {
    printf("\n\n");
    printf(
        "Execution error: Some threads still running in CTA during CTA "
        "reallocation! (1)\n");
    printf("   CTA uid = %Lu (sm_idx = %u) : %lu running out of %zu total\n",
           m_uid, m_sm_idx,
           (m_threads_in_cta.size() - m_threads_that_have_exited.size()),
           m_threads_in_cta.size());
    printf("   These are the threads that are still running:\n");
    std::set<ptx_thread_info *>::iterator t_iter;
    for (t_iter = m_threads_in_cta.begin(); t_iter != m_threads_in_cta.end();
         ++t_iter) {
      ptx_thread_info *t = *t_iter;
      if (m_threads_that_have_exited.find(t) ==
          m_threads_that_have_exited.end()) {
        if (m_dangling_pointers.find(t) != m_dangling_pointers.end()) {
          printf("       <thread deleted>\n");
        } else {
          printf("       [done=%c] : ", (t->is_done() ? 'Y' : 'N'));
          t->print_insn(t->get_pc(), stdout);
          printf("\n");
        }
      }
    }
    printf("\n\n");
    fail = true;
  }
  if (fail) {
    abort();
  }

  bool fail2 = false;
  std::set<ptx_thread_info *>::iterator t_iter;
  for (t_iter = m_threads_in_cta.begin(); t_iter != m_threads_in_cta.end();
       ++t_iter) {
    ptx_thread_info *t = *t_iter;
    if (m_dangling_pointers.find(t) == m_dangling_pointers.end()) {
      if (!t->is_done()) {
        if (!fail2) {
          printf(
              "Execution error: Some threads still running in CTA during CTA "
              "reallocation! (2)\n");
          printf("   CTA uid = %Lu (sm_idx = %u) :\n", m_uid, m_sm_idx);
          fail2 = true;
        }
        printf("       ");
        t->print_insn(t->get_pc(), stdout);
        printf("\n");
      }
    }
  }
  if (fail2) {
    abort();
  }
  m_threads_in_cta.clear();
  m_threads_that_have_exited.clear();
  m_dangling_pointers.clear();
}

void ptx_cta_info::register_thread_exit(ptx_thread_info *thd) {
  assert(m_threads_that_have_exited.find(thd) ==
         m_threads_that_have_exited.end());
  m_threads_that_have_exited.insert(thd);
}

void ptx_cta_info::register_deleted_thread(ptx_thread_info *thd) {
  m_dangling_pointers.insert(thd);
}

unsigned ptx_cta_info::get_sm_idx() const { return m_sm_idx; }

unsigned ptx_cta_info::get_bar_threads() const { return m_bar_threads; }

void ptx_cta_info::inc_bar_threads() { m_bar_threads++; }

void ptx_cta_info::reset_bar_threads() { m_bar_threads = 0; }

ptx_warp_info::ptx_warp_info() { reset_done_threads(); }

unsigned ptx_warp_info::get_done_threads() const { return m_done_threads; }

void ptx_warp_info::inc_done_threads() { m_done_threads++; }

void ptx_warp_info::reset_done_threads() { m_done_threads = 0; }

ptx_thread_info::~ptx_thread_info() {
  m_gpu->gpgpu_ctx->func_sim->g_ptx_thread_info_delete_count++;
}

extern unsigned g_regs_for_extra_cta;

ptx_thread_info::ptx_thread_info(kernel_info_t &kernel) : m_kernel(kernel) {
  m_uid = kernel.entry()->gpgpu_ctx->func_sim->g_ptx_thread_info_uid_next++;
  m_core = NULL;
  m_barrier_num = -1;
  m_at_barrier = false;
  m_valid = false;
  m_gridid = 0;
  m_thread_done = false;
  m_cycle_done = 0;
  m_PC = 0;
  m_icount = 0;
  m_last_effective_address = 0;
  m_last_memory_space = undefined_space;
  m_branch_taken = 0;
  m_shared_mem = NULL;
  m_sstarr_mem = NULL;
  m_warp_info = NULL;
  m_cta_info = NULL;
  m_local_mem = NULL;
  m_symbol_table = NULL;
  m_func_info = NULL;
  m_hw_tid = -1;
  m_hw_wid = -1;
  m_hw_sid = -1;
  m_last_dram_callback.function = NULL;
  m_last_dram_callback.instruction = NULL;
  m_regs.push_back(reg_map_t());
  assert(g_regs_for_extra_cta);
  m_extra_cta_regs = new RegSlot[g_regs_for_extra_cta];
  m_debug_trace_regs_modified.push_back(reg_map_t());
  m_debug_trace_regs_read.push_back(reg_map_t());
  m_callstack.push_back(stack_entry());
  m_RPC = -1;
  m_RPC_updated = false;
  m_last_was_call = false;
  m_enable_debug_trace = false;
  m_local_mem_stack_pointer = 0;
  m_gpu = NULL;
  m_last_set_operand_value = ptx_reg_t();
  ptx_cmpl = new ptx_compiler_components();
}

const ptx_version &ptx_thread_info::get_ptx_version() const {
  return m_func_info->get_ptx_version();
}

void ptx_thread_info::set_done() {
  assert(!m_at_barrier);
  m_thread_done = true;
  m_cycle_done = m_gpu->gpu_sim_cycle;
}

unsigned ptx_thread_info::get_builtin(int builtin_id, unsigned dim_mod) {
  assert(m_valid);
  switch ((builtin_id & 0xFFFF)) {
    case CLOCK_REG:
      return (unsigned)(m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
    case CLOCK64_REG:
      abort();  // change return value to unsigned long long?
                // GPGPUSim clock is 4 times slower - multiply by 4
      return (m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle) * 4;
    case HALFCLOCK_ID:
      // GPGPUSim clock is 4 times slower - multiply by 4
      // Hardware clock counter is incremented at half the shader clock
      // frequency - divide by 2 (Henry '10)
      return (m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle) * 2;
    case CTAID_REG:
      assert(dim_mod < 3);
      if (dim_mod == 0) return m_ctaid.x;
      if (dim_mod == 1) return m_ctaid.y;
      if (dim_mod == 2) return m_ctaid.z;
      abort();
      break;
    case ENVREG_REG: {
      int index = builtin_id >> 16;
      dim3 gdim = this->get_core()->get_kernel_info()->get_grid_dim();
      switch (index) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          return 0;
          break;
        case 6:
          return gdim.x;
        case 7:
          return gdim.y;
        case 8:
          return gdim.z;
        case 9:
          if (gdim.z == 1 && gdim.y == 1)
            return 1;
          else if (gdim.z == 1)
            return 2;
          else
            return 3;
          break;
        default:
          break;
      }
    }
    case GRIDID_REG:
      return m_gridid;
    case LANEID_REG:
      return get_hw_tid() % m_core->get_warp_size();
    case LANEMASK_EQ_REG:
      feature_not_implemented("%lanemask_eq");
      return 0;
    case LANEMASK_LE_REG:
      feature_not_implemented("%lanemask_le");
      return 0;
    case LANEMASK_LT_REG:
      feature_not_implemented("%lanemask_lt");
      return 0;
    case LANEMASK_GE_REG:
      feature_not_implemented("%lanemask_ge");
      return 0;
    case LANEMASK_GT_REG:
      feature_not_implemented("%lanemask_gt");
      return 0;
    case NCTAID_REG:
      assert(dim_mod < 3);
      if (dim_mod == 0) return m_nctaid.x;
      if (dim_mod == 1) return m_nctaid.y;
      if (dim_mod == 2) return m_nctaid.z;
      abort();
      break;
    case NTID_REG:
      assert(dim_mod < 3);
      if (dim_mod == 0) return m_ntid.x;
      if (dim_mod == 1) return m_ntid.y;
      if (dim_mod == 2) return m_ntid.z;
      abort();
      break;
    case NWARPID_REG:
      feature_not_implemented("%nwarpid");
      return 0;
    case PM_REG:
      feature_not_implemented("%pm");
      return 0;
    case SMID_REG:
      feature_not_implemented("%smid");
      return 0;
    case TID_REG:
      assert(dim_mod < 3);
      if (dim_mod == 0) return m_tid.x;
      if (dim_mod == 1) return m_tid.y;
      if (dim_mod == 2) return m_tid.z;
      abort();
      break;
    case WARPSZ_REG:
      return m_core->get_warp_size();
    default:
      assert(0);
  }
  return 0;
}

void ptx_thread_info::set_info(function_info *func) {
  m_symbol_table = func->get_symtab();
  m_func_info = func;
  m_PC = func->get_start_PC();
}

void ptx_thread_info::cpy_tid_to_reg(dim3 tid) {
  // copies %tid.x, %tid.y and %tid.z into $r0
  ptx_reg_t data;
  data.s64 = 0;

  data.u32 = (tid.x + (tid.y << 16) + (tid.z << 26));

  const symbol *r0 = m_symbol_table->lookup("$r0");
  if (r0) {
    // No need to set pid if kernel doesn't use it
    set_reg(r0, data);
  }
}

void ptx_thread_info::print_insn(unsigned pc, FILE *fp) const {
  m_func_info->print_insn(pc, fp);
}

static void print_reg(FILE *fp, std::string name, ptx_reg_t value,
                      symbol_table *symtab) {
  const symbol *sym = symtab->lookup(name.c_str());
  fprintf(fp, "  %8s   ", name.c_str());
  if (sym == NULL) {
    fprintf(fp, "<unknown type> 0x%llx\n", (unsigned long long)value.u64);
    return;
  }
  const type_info *t = sym->type();
  if (t == NULL) {
    fprintf(fp, "<unknown type> 0x%llx\n", (unsigned long long)value.u64);
    return;
  }
  type_info_key ti = t->get_key();

  switch (ti.scalar_type()) {
    case S8_TYPE:
      fprintf(fp, ".s8  %d\n", value.s8);
      break;
    case S16_TYPE:
      fprintf(fp, ".s16 %d\n", value.s16);
      break;
    case S32_TYPE:
      fprintf(fp, ".s32 %d\n", value.s32);
      break;
    case S64_TYPE:
      fprintf(fp, ".s64 %Ld\n", value.s64);
      break;
    case U8_TYPE:
      fprintf(fp, ".u8  %u [0x%02x]\n", value.u8, (unsigned)value.u8);
      break;
    case U16_TYPE:
      fprintf(fp, ".u16 %u [0x%04x]\n", value.u16, (unsigned)value.u16);
      break;
    case U32_TYPE:
      fprintf(fp, ".u32 %u [0x%08x]\n", value.u32, (unsigned)value.u32);
      break;
    case U64_TYPE:
      fprintf(fp, ".u64 %llu [0x%llx]\n", value.u64, value.u64);
      break;
    case F16_TYPE:
      fprintf(fp, ".f16 %f [0x%04x]\n", value.f16, (unsigned)value.u16);
      break;
    case F32_TYPE:
      fprintf(fp, ".f32 %.15lf [0x%08x]\n", value.f32, value.u32);
      break;
    case F64_TYPE:
      fprintf(fp, ".f64 %.15le [0x%016llx]\n", value.f64, value.u64);
      break;
    case B8_TYPE:
      fprintf(fp, ".b8  0x%02x\n", (unsigned)value.u8);
      break;
    case B16_TYPE:
      fprintf(fp, ".b16 0x%04x\n", (unsigned)value.u16);
      break;
    case B32_TYPE:
      fprintf(fp, ".b32 0x%08x\n", (unsigned)value.u32);
      break;
    case B64_TYPE:
      fprintf(fp, ".b64 0x%llx\n", (unsigned long long)value.u64);
      break;
    case PRED_TYPE:
      fprintf(fp, ".pred %u\n", (unsigned)value.pred);
      break;
    default:
      fprintf(fp, "non-scalar type\n");
      break;
  }
  fflush(fp);
}

static void print_reg(std::string name, ptx_reg_t value, symbol_table *symtab) {
  print_reg(stdout, name, value, symtab);
}

void ptx_thread_info::callstack_push(unsigned pc, unsigned rpc,
                                     const symbol *return_var_src,
                                     const symbol *return_var_dst,
                                     unsigned call_uid) {
  m_RPC = -1;
  m_RPC_updated = true;
  m_last_was_call = true;
  assert(m_func_info != NULL);
  m_callstack.push_back(stack_entry(m_symbol_table, m_func_info, pc, rpc,
                                    return_var_src, return_var_dst, call_uid));
  m_regs.push_back(reg_map_t());
  m_debug_trace_regs_modified.push_back(reg_map_t());
  m_debug_trace_regs_read.push_back(reg_map_t());
  m_local_mem_stack_pointer += m_func_info->local_mem_framesize();
}

// ptxplus version of callstack_push.
void ptx_thread_info::callstack_push_plus(unsigned pc, unsigned rpc,
                                          const symbol *return_var_src,
                                          const symbol *return_var_dst,
                                          unsigned call_uid) {
  m_RPC = -1;
  m_RPC_updated = true;
  m_last_was_call = true;
  assert(m_func_info != NULL);
  m_callstack.push_back(stack_entry(m_symbol_table, m_func_info, pc, rpc,
                                    return_var_src, return_var_dst, call_uid));
  // m_regs.push_back( reg_map_t() );
  // m_debug_trace_regs_modified.push_back( reg_map_t() );
  // m_debug_trace_regs_read.push_back( reg_map_t() );
  m_local_mem_stack_pointer += m_func_info->local_mem_framesize();
}

bool ptx_thread_info::callstack_pop() {
  const symbol *rv_src = m_callstack.back().m_return_var_src;
  const symbol *rv_dst = m_callstack.back().m_return_var_dst;
  assert(!((rv_src != NULL) ^
           (rv_dst != NULL)));  // ensure caller and callee agree on whether
                                // there is a return value

  // read return value from callee frame
  arg_buffer_t buffer(m_gpu->gpgpu_ctx);
  if (rv_src != NULL)
    buffer = copy_arg_to_buffer(this, operand_info(rv_src, m_gpu->gpgpu_ctx),
                                rv_dst);

  m_symbol_table = m_callstack.back().m_symbol_table;
  m_NPC = m_callstack.back().m_PC;
  m_RPC_updated = true;
  m_last_was_call = false;
  m_RPC = m_callstack.back().m_RPC;
  m_func_info = m_callstack.back().m_func_info;
  if (m_func_info) {
    assert(m_local_mem_stack_pointer >= m_func_info->local_mem_framesize());
    m_local_mem_stack_pointer -= m_func_info->local_mem_framesize();
  }
  m_callstack.pop_back();
  m_regs.pop_back();
  m_debug_trace_regs_modified.pop_back();
  m_debug_trace_regs_read.pop_back();

  // write return value into caller frame
  if (rv_dst != NULL) copy_buffer_to_frame(this, buffer);

  return m_callstack.empty();
}

// ptxplus version of callstack_pop
bool ptx_thread_info::callstack_pop_plus() {
  const symbol *rv_src = m_callstack.back().m_return_var_src;
  const symbol *rv_dst = m_callstack.back().m_return_var_dst;
  assert(!((rv_src != NULL) ^
           (rv_dst != NULL)));  // ensure caller and callee agree on whether
                                // there is a return value

  // read return value from callee frame
  arg_buffer_t buffer(m_gpu->gpgpu_ctx);
  if (rv_src != NULL)
    buffer = copy_arg_to_buffer(this, operand_info(rv_src, m_gpu->gpgpu_ctx),
                                rv_dst);

  m_symbol_table = m_callstack.back().m_symbol_table;
  m_NPC = m_callstack.back().m_PC;
  m_RPC_updated = true;
  m_last_was_call = false;
  m_RPC = m_callstack.back().m_RPC;
  m_func_info = m_callstack.back().m_func_info;
  if (m_func_info) {
    assert(m_local_mem_stack_pointer >= m_func_info->local_mem_framesize());
    m_local_mem_stack_pointer -= m_func_info->local_mem_framesize();
  }
  m_callstack.pop_back();
  // m_regs.pop_back();
  // m_debug_trace_regs_modified.pop_back();
  // m_debug_trace_regs_read.pop_back();

  // write return value into caller frame
  if (rv_dst != NULL) copy_buffer_to_frame(this, buffer);

  return m_callstack.empty();
}

void ptx_thread_info::dump_callstack() const {
  std::list<stack_entry>::const_iterator c = m_callstack.begin();
  std::list<reg_map_t>::const_iterator r = m_regs.begin();

  printf("\n\n");
  printf("Call stack for thread uid = %u (sc=%u, hwtid=%u)\n", m_uid, m_hw_sid,
         m_hw_tid);
  while (c != m_callstack.end() && r != m_regs.end()) {
    const stack_entry &c_e = *c;
    const reg_map_t &regs = *r;
    if (!c_e.m_valid) {
      printf("  <entry>                              #regs = %zu\n",
             regs.size());
    } else {
      printf("  %20s  PC=%3u RV= (callee=\'%s\',caller=\'%s\') #regs = %zu\n",
             c_e.m_func_info->get_name().c_str(), c_e.m_PC,
             c_e.m_return_var_src->name().c_str(),
             c_e.m_return_var_dst->name().c_str(), regs.size());
    }
    c++;
    r++;
  }
  if (c != m_callstack.end() || r != m_regs.end()) {
    printf("  *** mismatch in m_regs and m_callstack sizes ***\n");
  }
  printf("\n\n");
}

std::string ptx_thread_info::get_location() const {
  const ptx_instruction *pI = m_func_info->get_instruction(m_PC);
  char buf[1024];
  snprintf(buf, 1024, "%s:%u", pI->source_file(), pI->source_line());
  return std::string(buf);
}

const ptx_instruction *ptx_thread_info::get_inst() const {
  return m_func_info->get_instruction(m_PC);
}

const ptx_instruction *ptx_thread_info::get_inst(addr_t pc) const {
  return m_func_info->get_instruction(pc);
}

void ptx_thread_info::dump_regs(FILE *fp) {
  if (m_regs.empty()) return;
  if (m_regs.back().empty()) return;
  fprintf(fp, "Register File Contents:\n");
  fflush(fp);
  reg_map_t::const_iterator r;
  for (r = m_regs.back().begin(); r != m_regs.back().end(); ++r) {
    const symbol *sym = r->first;
    ptx_reg_t value = r->second;
    std::string name = sym->name();
    print_reg(fp, name, value, m_symbol_table);
  }
}

void ptx_thread_info::dump_modifiedregs(FILE *fp) {
  if (!(m_debug_trace_regs_modified.empty() ||
        m_debug_trace_regs_modified.back().empty())) {
    fprintf(fp, "Output Registers:\n");
    fflush(fp);
    reg_map_t::iterator r;
    for (r = m_debug_trace_regs_modified.back().begin();
         r != m_debug_trace_regs_modified.back().end(); ++r) {
      const symbol *sym = r->first;
      std::string name = sym->name();
      ptx_reg_t value = r->second;
      print_reg(fp, name, value, m_symbol_table);
    }
  }
  if (!(m_debug_trace_regs_read.empty() ||
        m_debug_trace_regs_read.back().empty())) {
    fprintf(fp, "Input Registers:\n");
    fflush(fp);
    reg_map_t::iterator r;
    for (r = m_debug_trace_regs_read.back().begin();
         r != m_debug_trace_regs_read.back().end(); ++r) {
      const symbol *sym = r->first;
      std::string name = sym->name();
      ptx_reg_t value = r->second;
      print_reg(fp, name, value, m_symbol_table);
    }
  }
}

void ptx_thread_info::push_breakaddr(const operand_info &breakaddr) {
  m_breakaddrs.push(breakaddr);
}

const operand_info &ptx_thread_info::pop_breakaddr() {
  if (m_breakaddrs.empty()) {
    printf("empty breakaddrs stack");
    assert(0);
  }
  operand_info &breakaddr = m_breakaddrs.top();
  m_breakaddrs.pop();
  return breakaddr;
}

void ptx_thread_info::set_npc(const function_info *f) {
  m_NPC = f->get_start_PC();
  m_func_info = const_cast<function_info *>(f);
  m_symbol_table = m_func_info->get_symtab();
}

// for write (set_reg)
int ptx_thread_info::lookup_rid_for_write(int reg_num, int& dict) {
  int rid = (reg_num-1) % g_regs_for_extra_cta;
  assert(rid >= 0 && rid < g_regs_for_extra_cta);
  dict = 0;

  // TODO: 1 compare op. per clock cycle 
  // reg slot (pointed by rid) is occupied
  // for write case, reg_num points to an empty slot
  while (m_extra_cta_regs[rid].valid && 
    m_extra_cta_regs[rid].reg_num != reg_num) {
    rid = (rid+1) % g_regs_for_extra_cta;
    dict++;
    if (dict == g_regs_for_extra_cta) {
      rid = -1;
      break;
    }
  }
  return rid;
  
}

// for read (get_reg)
int ptx_thread_info::lookup_rid_for_read(int reg_num) {
  int rid = (reg_num-1) % g_regs_for_extra_cta;
  assert(rid >= 0 && rid < g_regs_for_extra_cta);
  int dict = 0;

  // TODO: 1 compare op. per clock cycle 
  // find reg_num in m_extra_cta_regs
  // for read case, the reg_num must have existed in RF
  while (dict < g_regs_for_extra_cta) {
    if (m_extra_cta_regs[rid].reg_num == reg_num
      && m_extra_cta_regs[rid].valid) {
        return rid;
    }
    rid = (rid+1) % g_regs_for_extra_cta;
    dict++;
  }
  // cannot find reg_num, namely reg_num doesn't exist
  return -1;
}

// for read (get_reg)
int ptx_thread_info::lookup_rid_for_valid(int reg_num) {
  int rid = (reg_num-1) % g_regs_for_extra_cta;
  assert(rid >= 0 && rid < g_regs_for_extra_cta);
  int start_rid = rid;
  int dict = 0;

  // TODO: 1 compare op. per clock cycle 
  // check if reg_num already exist and be validated
  while (dict < g_regs_for_extra_cta) {
    if (m_extra_cta_regs[rid].reg_num == reg_num
      && m_extra_cta_regs[rid].valid) {
        return rid;
    }
    rid = (rid+1) % g_regs_for_extra_cta;
    dict++;
  }

  // reg_num doesn't exist, should find an empty slot for it
  rid = start_rid, dict = 0;
  while (m_extra_cta_regs[rid].valid && dict < g_regs_for_extra_cta) {
    rid = (rid+1) % g_regs_for_extra_cta;
    dict++;
  }
  // find an empty slot for the reg_num
  if (dict < g_regs_for_extra_cta)
    return rid;
  // no empty slot for reg_num, the m_extra_cta_regs is full
  return -1; 
}

// for set_reg_valid
void ptx_thread_info::set_reg_valid() {
  if (!liveSet.size()) {
    std::list<ptx_instruction*> instrs_list = m_func_info->get_insn();
    std::vector<ptx_instruction*> instrs(instrs_list.begin(), instrs_list.end());
    liveSet = ptx_cmpl->get_liveness(instrs);
  }
  int pc = get_pc() / 8;
  std::vector<operand_info*> inst_liveness;
  inst_liveness = liveSet[pc];

  std::set<int> prev_liveness; // vector<rid>
  std::set<int> curr_liveness;
  std::vector<int> delta_deadreg; // prev - curr

  // printf("<<PC: %d>> ",pc);
  // printf("\n<prev live regs> ");
  // get previous liveness
  for (int i = 0; i < g_regs_for_extra_cta; ++i) {
    if (m_extra_cta_regs[i].valid) {
      prev_liveness.insert(i);
      // printf("%d ",i);// for test
    }
  }
  // printf("\n");
  // printf("<cur live regs> ");

  // set live regs
  for(int i = 0; i < inst_liveness.size(); ++i) {
    operand_info* oprd = inst_liveness[i];
    int reg_num = oprd->reg_num();
    int rid = lookup_rid_for_valid(reg_num);
    assert(rid != -1);
    m_extra_cta_regs[rid].valid = true;
    curr_liveness.insert(rid); // get current liveness
    // printf("%d(%s) ", rid, oprd->name().c_str());
  }
  // printf("\n<dead regs> ");

  std::set_difference(prev_liveness.begin(), prev_liveness.end(),
    curr_liveness.begin(), curr_liveness.end(), std::inserter(
    delta_deadreg, delta_deadreg.begin()));
  // reset dead regs
  for (int rid = 0; rid < delta_deadreg.size(); ++rid) {
    m_extra_cta_regs[delta_deadreg[rid]].valid = false;
    // printf("%d ", delta_deadreg[rid]); // for test
  }

  // printf("\n[After Validation] m_extra_cta_regs: ");
  // for(int i=0; i<g_regs_for_extra_cta; ++i){
  //   if (m_extra_cta_regs[i].valid)
  //     printf("%d ", m_extra_cta_regs[i].reg_num);
  //   else printf("_ ");
  // }
  // printf("\n");
}

void feature_not_implemented(const char *f) {
  printf("GPGPU-Sim: feature '%s' not supported\n", f);
  abort();
}