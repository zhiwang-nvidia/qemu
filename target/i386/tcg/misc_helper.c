/*
 *  x86 misc helpers
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/exec-all.h"
#include "helper-tcg.h"
#include <stdio.h>
static bool Debug = true;

/*
 * NOTE: the translator must set DisasContext.cc_op to CC_OP_EFLAGS
 * after generating a call to a helper that uses this.
 */
void cpu_load_eflags(CPUX86State *env, int eflags, int update_mask)
{
    CC_SRC = eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    CC_OP = CC_OP_EFLAGS;
    env->df = 1 - (2 * ((eflags >> 10) & 1));
    env->eflags = (env->eflags & ~update_mask) |
        (eflags & update_mask) | 0x2;
}

void helper_into(CPUX86State *env, int next_eip_addend)
{
    int eflags;

    eflags = cpu_cc_compute_all(env, CC_OP);
    if (eflags & CC_O) {
        raise_interrupt(env, EXCP04_INTO, 1, 0, next_eip_addend);
    }
}

void helper_cpuid(CPUX86State *env)
{
    uint32_t eax, ebx, ecx, edx;

    cpu_svm_check_intercept_param(env, SVM_EXIT_CPUID, 0, GETPC());

    cpu_x86_cpuid(env, (uint32_t)env->regs[R_EAX], (uint32_t)env->regs[R_ECX],
                  &eax, &ebx, &ecx, &edx);
    env->regs[R_EAX] = eax;
    env->regs[R_EBX] = ebx;
    env->regs[R_ECX] = ecx;
    env->regs[R_EDX] = edx;
}

void helper_rdtsc(CPUX86State *env) // ？？？ 读取时间相关的函数
{
    uint64_t val;

    if ((env->cr[4] & CR4_TSD_MASK) && ((env->hflags & HF_CPL_MASK) != 0)) {
        raise_exception_ra(env, EXCP0D_GPF, GETPC());
    }
    cpu_svm_check_intercept_param(env, SVM_EXIT_RDTSC, 0, GETPC());

    val = cpu_get_tsc(env) + env->tsc_offset;
    env->regs[R_EAX] = (uint32_t)(val);
    env->regs[R_EDX] = (uint32_t)(val >> 32);
}

void helper_senduipi(CPUX86State *env ,int reg_index){ // 改
    // CPUState *cs = env_cpu(env);
    int uitte_index = env->regs[R_EAX];
    if(Debug)printf("qemu:helper senduipi called receive  regidx:%d, uipiindex: %d\n",reg_index,uitte_index);
    int prot;
    CPUState *cs = env_cpu(env);

    // read tempUITTE from 16 bytes at UITTADDR+ (reg « 4);
    uint64_t uitt_phyaddress = get_hphys2(cs, (env->uintr_tt>>3)<<3 , MMU_DATA_LOAD, &prot);
    if(Debug) printf("uitt_phyaddress %lx \n", uitt_phyaddress);
    uint64_t content = x86_ldq_phys(cs,uitt_phyaddress + (uitte_index<<4));
    uint64_t upidaddress = x86_ldq_phys(cs, uitt_phyaddress + (uitte_index<<4) + 8);
    if(Debug)printf("data of uitt %d is 0x%016lx\n",uitte_index, content);
    if(Debug)printf("UPID address 0x%016lx\n", upidaddress);

    // tempUPID.PIR[tempUITTE.UV] := 1;
    uint64_t upid_phyaddress = get_hphys2(cs, upidaddress , MMU_DATA_LOAD, &prot);
    uint64_t upid_content = x86_ldq_phys(cs, upid_phyaddress);
    if(Debug)printf("content of uipid: 0x%016lx\n", upid_content);
    uint64_t SET_UV1 = 1<<8;
    upid_content |= SET_UV1;
    x86_stq_phys(cs, upid_phyaddress, upid_content);


    // uint64_t content = x86_ldq_phys(cs,(env->uintr_tt>>3)<<3);
    // if(Debug)printf("data of uitt0is 0x%016lx\n",content);

    // if(Debug)printf("qemu:helper senduipi called receive  regidx:%d, uipiindex: %d\n",reg_index,uipi_index);
    // uint64_t content = cpu_ldq_data_ra(env, (env->uintr_tt>>3)<<3,0);
    // if(Debug)printf("data of uitt0is 0x%016lx\n",content);
    
}

void helper_rdtscp(CPUX86State *env)
{
    helper_rdtsc(env);
    env->regs[R_ECX] = (uint32_t)(env->tsc_aux);
}

void QEMU_NORETURN helper_rdpmc(CPUX86State *env)
{
    if (((env->cr[4] & CR4_PCE_MASK) == 0 ) &&
        ((env->hflags & HF_CPL_MASK) != 0)) {
        raise_exception_ra(env, EXCP0D_GPF, GETPC());
    }
    cpu_svm_check_intercept_param(env, SVM_EXIT_RDPMC, 0, GETPC());

    /* currently unimplemented */
    qemu_log_mask(LOG_UNIMP, "x86: unimplemented rdpmc\n");
    raise_exception_err(env, EXCP06_ILLOP, 0);
}

void QEMU_NORETURN do_pause(CPUX86State *env)
{
    CPUState *cs = env_cpu(env);

    /* Just let another CPU run.  */
    cs->exception_index = EXCP_INTERRUPT;
    cpu_loop_exit(cs);
}

void QEMU_NORETURN helper_pause(CPUX86State *env, int next_eip_addend)
{
    cpu_svm_check_intercept_param(env, SVM_EXIT_PAUSE, 0, GETPC());
    env->eip += next_eip_addend;

    do_pause(env);
}

uint64_t helper_rdpkru(CPUX86State *env, uint32_t ecx)
{
    if ((env->cr[4] & CR4_PKE_MASK) == 0) {
        raise_exception_err_ra(env, EXCP06_ILLOP, 0, GETPC());
    }
    if (ecx != 0) {
        raise_exception_err_ra(env, EXCP0D_GPF, 0, GETPC());
    }

    return env->pkru;
}

void helper_wrpkru(CPUX86State *env, uint32_t ecx, uint64_t val)
{
    CPUState *cs = env_cpu(env);

    if ((env->cr[4] & CR4_PKE_MASK) == 0) {
        raise_exception_err_ra(env, EXCP06_ILLOP, 0, GETPC());
    }
    if (ecx != 0 || (val & 0xFFFFFFFF00000000ull)) {
        raise_exception_err_ra(env, EXCP0D_GPF, 0, GETPC());
    }

    env->pkru = val;
    tlb_flush(cs);
}
