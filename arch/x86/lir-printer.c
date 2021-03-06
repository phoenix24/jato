/*
 * Copyright (c) 2009  Arthur Huillet
 * Copyright (c) 2006-2008  Pekka Enberg
 *
 * This file is released under the 2-clause BSD license. Please refer to the
 * file LICENSE for details.
 */

#include "jit/compilation-unit.h"
#include "jit/basic-block.h"
#include "jit/lir-printer.h"
#include "jit/emit-code.h"
#include "jit/statement.h"
#include "jit/compiler.h"
#include "jit/vars.h"

#include "vm/method.h"
#include "vm/die.h"

#include "lib/buffer.h"
#include "lib/string.h"
#include "lib/list.h"

#include "arch/instruction.h"
#include "arch/memory.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

static inline int print_imm(struct string *str, struct operand *op)
{
	return str_append(str, "$0x%lx", op->imm);
}

static inline int print_reg(struct string *str, struct operand *op)
{
	struct live_interval *interval = op->reg.interval;

	if (interval_has_fixed_reg(interval))
		return str_append(str, "r%lu=%s", interval->var_info->vreg,
				  reg_name(interval->reg));
	else
		return str_append(str, "r%lu", interval->var_info->vreg);
}

static inline int print_membase(struct string *str, struct operand *op)
{
	return str_append(str, "$0x%lx(r%lu)", op->disp, op->base_reg.interval->var_info->vreg);
}

static inline int print_memdisp(struct string *str, struct operand *op)
{
	return str_append(str, "($0x%lx)", op->disp);
}

static inline int print_memlocal(struct string *str, struct operand *op)
{
	return str_append(str, "@%ld(bp)", op->slot->index);
}

static inline int print_memindex(struct string *str, struct operand *op)
{
	return str_append(str, "(r%lu, r%lu, %d)", op->base_reg.interval->var_info->vreg, op->index_reg.interval->var_info->vreg, op->shift);
}

static inline int print_rel(struct string *str, struct operand *op)
{
	return str_append(str, "$0x%lx", op->rel);
}

static inline int print_branch(struct string *str, struct operand *op)
{
	return str_append(str, "bb 0x%lx", op->branch_target);
}

static int print_imm_reg(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_imm_membase(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_membase(str, &insn->dest);
}

static int print_imm_memlocal(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_memlocal(str, &insn->dest);
}

static int print_imm_memdisp(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_memdisp(str, &insn->dest);
}

static int print_membase_reg(struct string *str, struct insn *insn)
{
	print_membase(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_memdisp_reg(struct string *str, struct insn *insn)
{
	str_append(str, "(");
	print_imm(str, &insn->src);
	str_append(str, "), ");
	return print_reg(str, &insn->dest);
}

static int print_reg_memdisp(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", (");
	print_imm(str, &insn->dest);
	return str_append(str, ")");
}

static int print_tlmemdisp_reg(struct string *str, struct insn *insn)
{
	str_append(str, "gs:(");
	print_imm(str, &insn->src);
	str_append(str, "), ");
	return print_reg(str, &insn->dest);
}

static int print_tlmembase(struct string *str, struct operand *op)
{
	str_append(str, "gs:(");
	print_membase(str, op);
	return str_append(str, ")");
}

static int print_tlmemdisp(struct string *str, struct operand *op)
{
	str_append(str, "gs:(");
	print_imm(str, op);
	return str_append(str, ")");
}

static int print_reg_tlmemdisp(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_tlmemdisp(str, &insn->dest);
}

static int print_imm_tlmembase(struct string *str, struct insn *insn)
{
	print_imm(str, &insn->src);
	str_append(str, ", ");
	return print_tlmembase(str, &insn->dest);
}

static int print_reg_tlmembase(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_tlmembase(str, &insn->dest);
}

static int print_reg_membase(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_membase(str, &insn->dest);
}

static int print_memlocal_reg(struct string *str, struct insn *insn)
{
	print_memlocal(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_memindex_reg(struct string *str, struct insn *insn)
{
	print_memindex(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_reg_memlocal(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_memlocal(str, &insn->dest);
}

static int print_reg_memindex(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_memindex(str, &insn->dest);
}

static int print_reg_reg(struct string *str, struct insn *insn)
{
	print_reg(str, &insn->src);
	str_append(str, ", ");
	return print_reg(str, &insn->dest);
}

static int print_reg_regs(struct string *str, struct insn *insn)
{
	for (unsigned long ndx = 0; ndx < insn->nr_srcs; ndx++) {
		print_reg(str, &insn->ssa_srcs[ndx]);
		str_append(str, ", ");
	}
	return print_reg(str, &insn->ssa_dest);
}

#define print_func_name(str) str_append(str, "%-20s ", __func__ + 6)

static int print_adc_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_adc_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_addss_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_addsd_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_adc_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_add_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_add_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_add_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_subss_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_subsd_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_mulss_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_mulsd_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fmul_64_memdisp_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memdisp_reg(str, insn);
}

static int print_divss_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_divsd_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_fld_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fld_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->operand);
}

static int print_fld_64_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fld_64_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->operand);
}

static int print_fild_64_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fstp_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fstp_64_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fstp_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->operand);
}

static int print_ic_call(struct string *str, struct insn *insn)
{
	print_func_name(str);
	print_reg(str, &insn->src);
	str_append(str, ", ");
	print_imm(str, &insn->dest);
	return str_append(str, "<%s>", ((struct vm_method *)insn->dest.imm)->name);
}

static int print_fstp_64_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->operand);
}

static int print_fnstcw_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fldcw_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_fistp_64_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->operand);
}

static int print_and_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_and_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_call_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	str_append(str, "(");
	print_reg(str, &insn->dest);
	return str_append(str, ")");
}

static int print_call_rel(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_rel(str, &insn->operand);
}

static int print_cltd_reg_reg(struct string *str, struct insn *insn)	/* CDQ in Intel manuals*/
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_cmp_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_cmp_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_cmp_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_div_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_div_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_movss_membase_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_movsd_membase_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_movss_xmm_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_membase(str, insn);
}

static int print_movsd_xmm_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_membase(str, insn);
}

static int print_conv_fpu_to_gpr(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_conv_fpu64_to_gpr(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_conv_gpr_to_fpu(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_conv_gpr_to_fpu64(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_conv_xmm_to_xmm64(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_conv_xmm64_to_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_je_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jge_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jg_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jle_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jl_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jmp_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_jmp_memindex(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memindex(str, &insn->dest);
}

static int print_jmp_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase(str, &insn->dest);
}

static int print_jne_branch(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_branch(str, &insn->operand);
}

static int print_mov_imm_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_membase(str, insn);
}

static int print_mov_imm_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_memlocal(str, insn);
}

static int print_mov_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_mov_memlocal_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal_reg(str, insn);
}

static int print_movss_memlocal_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal_reg(str, insn);
}

static int print_movsd_memlocal_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal_reg(str, insn);
}

static int print_mov_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_mov_memdisp_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memdisp_reg(str, insn);
}

static int print_movss_memdisp_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memdisp_reg(str, insn);
}

static int print_movsd_memdisp_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memdisp_reg(str, insn);
}

static int print_mov_reg_memdisp(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memdisp(str, insn);
}

static int print_movss_xmm_memdisp(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memdisp(str, insn);
}

static int print_movsd_xmm_memdisp(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memdisp(str, insn);
}

static int print_mov_tlmemdisp_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_tlmemdisp_reg(str, insn);
}

static int print_mov_reg_tlmemdisp(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_tlmemdisp(str, insn);
}

static int print_mov_imm_tlmembase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_tlmembase(str, insn);
}

static int print_mov_reg_tlmembase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_tlmembase(str, insn);
}

static int print_mov_memindex_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memindex_reg(str, insn);
}

static int print_movss_memindex_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memindex_reg(str, insn);
}

static int print_movsd_memindex_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memindex_reg(str, insn);
}

static int print_mov_reg_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_membase(str, insn);
}

static int print_mov_reg_memindex(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memindex(str, insn);
}

static int print_movss_xmm_memindex(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memindex(str, insn);
}

static int print_movsd_xmm_memindex(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memindex(str, insn);
}

static int print_mov_reg_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memlocal(str, insn);
}

static int print_movss_xmm_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memlocal(str, insn);
}

static int print_movsd_xmm_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_memlocal(str, insn);
}

static int print_mov_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_movss_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_movsd_xmm_xmm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_movsx_8_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	print_reg_reg(str, insn);
	return str_append(str, "(8bit->32bit)");
}

static int print_movsx_16_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	print_reg_reg(str, insn);
	return str_append(str, "(16bit->32bit)");
}

static int print_movzx_16_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	print_reg_reg(str, insn);
	return str_append(str, "(16bit->32bit)");
}

static int print_mul_membase_eax(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_mul_reg_eax(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_mul_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_neg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg(str, &insn->dest);
}

static int print_nop(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_or_imm_membase(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_membase(str, insn);
}

static int print_or_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_or_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_phi(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_regs(str, insn);
}

static int print_push_imm(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm(str, &insn->operand);
}

static int print_push_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg(str, &insn->operand);
}

static int print_push_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->src);
}

static int print_pop_memlocal(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_memlocal(str, &insn->src);
}

static int print_pop_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg(str, &insn->dest);
}

static int print_ret(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_sar_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_sar_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_sbb_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_sbb_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_sbb_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_shl_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_shr_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_sub_imm_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_reg(str, insn);
}

static int print_sub_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_sub_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_test_imm_memdisp(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_imm_memdisp(str, insn);
}

static int print_test_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_xor_membase_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_membase_reg(str, insn);
}

static int print_xor_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_xor_xmm_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_xor_64_xmm_reg_reg(struct string *str, struct insn *insn)
{
	print_func_name(str);
	return print_reg_reg(str, insn);
}

static int print_save_caller_regs(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_restore_caller_regs(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_restore_caller_regs_i32(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_restore_caller_regs_i64(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_restore_caller_regs_f32(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

static int print_restore_caller_regs_f64(struct string *str, struct insn *insn)
{
	return print_func_name(str);
}

typedef int (*print_insn_fn) (struct string *str, struct insn *insn);

static print_insn_fn insn_printers[] = {
	[INSN_ADC_IMM_REG] = print_adc_imm_reg,
	[INSN_ADC_MEMBASE_REG] = print_adc_membase_reg,
	[INSN_ADC_REG_REG] = print_adc_reg_reg,
	[INSN_ADDSD_XMM_XMM] = print_addsd_xmm_xmm,
	[INSN_ADDSS_XMM_XMM] = print_addss_xmm_xmm,
	[INSN_ADD_IMM_REG] = print_add_imm_reg,
	[INSN_ADD_MEMBASE_REG] = print_add_membase_reg,
	[INSN_ADD_REG_REG] = print_add_reg_reg,
	[INSN_AND_MEMBASE_REG] = print_and_membase_reg,
	[INSN_AND_REG_REG] = print_and_reg_reg,
	[INSN_CALL_REG] = print_call_reg,
	[INSN_CALL_REL] = print_call_rel,
	[INSN_CLTD_REG_REG] = print_cltd_reg_reg,	/* CDQ in Intel manuals*/
	[INSN_CMP_IMM_REG] = print_cmp_imm_reg,
	[INSN_CMP_MEMBASE_REG] = print_cmp_membase_reg,
	[INSN_CMP_REG_REG] = print_cmp_reg_reg,
	[INSN_CONV_FPU64_TO_GPR] = print_conv_fpu64_to_gpr,
	[INSN_CONV_FPU_TO_GPR] = print_conv_fpu_to_gpr,
	[INSN_CONV_GPR_TO_FPU64] = print_conv_gpr_to_fpu64,
	[INSN_CONV_GPR_TO_FPU] = print_conv_gpr_to_fpu,
	[INSN_CONV_XMM64_TO_XMM] = print_conv_xmm64_to_xmm,
	[INSN_CONV_XMM_TO_XMM64] = print_conv_xmm_to_xmm64,
	[INSN_DIVSD_XMM_XMM] = print_divsd_xmm_xmm,
	[INSN_DIVSS_XMM_XMM] = print_divss_xmm_xmm,
	[INSN_DIV_MEMBASE_REG] = print_div_membase_reg,
	[INSN_DIV_REG_REG] = print_div_reg_reg,
	[INSN_FILD_64_MEMBASE] = print_fild_64_membase,
	[INSN_FISTP_64_MEMBASE] = print_fistp_64_membase,
	[INSN_FLDCW_MEMBASE] = print_fldcw_membase,
	[INSN_FLD_64_MEMBASE] = print_fld_64_membase,
	[INSN_FLD_64_MEMLOCAL] = print_fld_64_memlocal,
	[INSN_FLD_MEMBASE] = print_fld_membase,
	[INSN_FLD_MEMLOCAL] = print_fld_memlocal,
	[INSN_FNSTCW_MEMBASE] = print_fnstcw_membase,
	[INSN_FSTP_64_MEMBASE] = print_fstp_64_membase,
	[INSN_FSTP_64_MEMLOCAL] = print_fstp_64_memlocal,
	[INSN_FSTP_MEMBASE] = print_fstp_membase,
	[INSN_FSTP_MEMLOCAL] = print_fstp_memlocal,
	[INSN_IC_CALL] = print_ic_call,
	[INSN_JE_BRANCH] = print_je_branch,
	[INSN_JGE_BRANCH] = print_jge_branch,
	[INSN_JG_BRANCH] = print_jg_branch,
	[INSN_JLE_BRANCH] = print_jle_branch,
	[INSN_JL_BRANCH] = print_jl_branch,
	[INSN_JMP_BRANCH] = print_jmp_branch,
	[INSN_JMP_MEMBASE] = print_jmp_membase,
	[INSN_JMP_MEMINDEX] = print_jmp_memindex,
	[INSN_JNE_BRANCH] = print_jne_branch,
	[INSN_MOVSD_MEMBASE_XMM] = print_movsd_membase_xmm,
	[INSN_MOVSD_MEMDISP_XMM] = print_movsd_memdisp_xmm,
	[INSN_MOVSD_MEMINDEX_XMM] = print_movsd_memindex_xmm,
	[INSN_MOVSD_MEMLOCAL_XMM] = print_movsd_memlocal_xmm,
	[INSN_MOVSD_XMM_MEMBASE] = print_movsd_xmm_membase,
	[INSN_MOVSD_XMM_MEMDISP] = print_movsd_xmm_memdisp,
	[INSN_MOVSD_XMM_MEMINDEX] = print_movsd_xmm_memindex,
	[INSN_MOVSD_XMM_MEMLOCAL] = print_movsd_xmm_memlocal,
	[INSN_MOVSD_XMM_XMM] = print_movsd_xmm_xmm,
	[INSN_MOVSS_MEMBASE_XMM] = print_movss_membase_xmm,
	[INSN_MOVSS_MEMDISP_XMM] = print_movss_memdisp_xmm,
	[INSN_MOVSS_MEMINDEX_XMM] = print_movss_memindex_xmm,
	[INSN_MOVSS_MEMLOCAL_XMM] = print_movss_memlocal_xmm,
	[INSN_MOVSS_XMM_MEMBASE] = print_movss_xmm_membase,
	[INSN_MOVSS_XMM_MEMDISP] = print_movss_xmm_memdisp,
	[INSN_MOVSS_XMM_MEMINDEX] = print_movss_xmm_memindex,
	[INSN_MOVSS_XMM_MEMLOCAL] = print_movss_xmm_memlocal,
	[INSN_MOVSS_XMM_XMM] = print_movss_xmm_xmm,
	[INSN_MOVSX_16_REG_REG] = print_movsx_16_reg_reg,
	[INSN_MOVSX_8_REG_REG] = print_movsx_8_reg_reg,
	[INSN_MOVZX_16_REG_REG] = print_movzx_16_reg_reg,
	[INSN_MOV_IMM_MEMBASE] = print_mov_imm_membase,
	[INSN_MOV_IMM_MEMLOCAL] = print_mov_imm_memlocal,
	[INSN_MOV_IMM_REG] = print_mov_imm_reg,
	[INSN_MOV_IMM_THREAD_LOCAL_MEMBASE] = print_mov_imm_tlmembase,
	[INSN_MOV_MEMBASE_REG] = print_mov_membase_reg,
	[INSN_MOV_MEMDISP_REG] = print_mov_memdisp_reg,
	[INSN_MOV_MEMINDEX_REG] = print_mov_memindex_reg,
	[INSN_MOV_MEMLOCAL_REG] = print_mov_memlocal_reg,
	[INSN_MOV_REG_MEMBASE] = print_mov_reg_membase,
	[INSN_MOV_REG_MEMDISP] = print_mov_reg_memdisp,
	[INSN_MOV_REG_MEMINDEX] = print_mov_reg_memindex,
	[INSN_MOV_REG_MEMLOCAL] = print_mov_reg_memlocal,
	[INSN_MOV_REG_REG] = print_mov_reg_reg,
	[INSN_MOV_REG_THREAD_LOCAL_MEMBASE] = print_mov_reg_tlmembase,
	[INSN_MOV_REG_THREAD_LOCAL_MEMDISP] = print_mov_reg_tlmemdisp,
	[INSN_MOV_THREAD_LOCAL_MEMDISP_REG] = print_mov_tlmemdisp_reg,
	[INSN_MULSD_MEMDISP_XMM] = print_fmul_64_memdisp_xmm,
	[INSN_MULSD_XMM_XMM] = print_mulsd_xmm_xmm,
	[INSN_MULSS_XMM_XMM] = print_mulss_xmm_xmm,
	[INSN_MUL_MEMBASE_EAX] = print_mul_membase_eax,
	[INSN_MUL_REG_EAX] = print_mul_reg_eax,
	[INSN_MUL_REG_REG] = print_mul_reg_reg,
	[INSN_NEG_REG] = print_neg_reg,
	[INSN_NOP] = print_nop,
	[INSN_OR_IMM_MEMBASE] = print_or_imm_membase,
	[INSN_OR_MEMBASE_REG] = print_or_membase_reg,
	[INSN_OR_REG_REG] = print_or_reg_reg,
	[INSN_PHI] = print_phi,
	[INSN_POP_MEMLOCAL] = print_pop_memlocal,
	[INSN_POP_REG] = print_pop_reg,
	[INSN_PUSH_IMM] = print_push_imm,
	[INSN_PUSH_MEMLOCAL] = print_push_memlocal,
	[INSN_PUSH_REG] = print_push_reg,
	[INSN_RET] = print_ret,
	[INSN_SAR_IMM_REG] = print_sar_imm_reg,
	[INSN_SAR_REG_REG] = print_sar_reg_reg,
	[INSN_SBB_IMM_REG] = print_sbb_imm_reg,
	[INSN_SBB_MEMBASE_REG] = print_sbb_membase_reg,
	[INSN_SBB_REG_REG] = print_sbb_reg_reg,
	[INSN_SHL_REG_REG] = print_shl_reg_reg,
	[INSN_SHR_REG_REG] = print_shr_reg_reg,
	[INSN_SUBSD_XMM_XMM] = print_subsd_xmm_xmm,
	[INSN_SUBSS_XMM_XMM] = print_subss_xmm_xmm,
	[INSN_SUB_IMM_REG] = print_sub_imm_reg,
	[INSN_SUB_MEMBASE_REG] = print_sub_membase_reg,
	[INSN_SUB_REG_REG] = print_sub_reg_reg,
	[INSN_TEST_IMM_MEMDISP] = print_test_imm_memdisp,
	[INSN_TEST_MEMBASE_REG] = print_test_membase_reg,
	[INSN_XORPD_XMM_XMM] = print_xor_64_xmm_reg_reg,
	[INSN_XORPS_XMM_XMM] = print_xor_xmm_reg_reg,
	[INSN_XOR_MEMBASE_REG] = print_xor_membase_reg,
	[INSN_XOR_REG_REG] = print_xor_reg_reg,
	[INSN_SAVE_CALLER_REGS] = print_save_caller_regs,
	[INSN_RESTORE_CALLER_REGS] = print_restore_caller_regs,
	[INSN_RESTORE_CALLER_REGS_I32] = print_restore_caller_regs_i32,
	[INSN_RESTORE_CALLER_REGS_I64] = print_restore_caller_regs_i64,
	[INSN_RESTORE_CALLER_REGS_F32] = print_restore_caller_regs_f32,
	[INSN_RESTORE_CALLER_REGS_F64] = print_restore_caller_regs_f64,
};

int lir_print(struct insn *insn, struct string *str)
{
	print_insn_fn print = insn_printers[insn->type];

	if (print == NULL)
		return warn("unknown insn %d\n", insn->type), -EINVAL;

	return print(str, insn);
}
