/*
* Copyright (C) 2012, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

@void vadd_neon(const float* source1P, const float* source2P, float* destP, size_t framesToProcess)
    .text
    .fpu neon
    .align 4
    .global     vadd_neon
    .func       vadd_neon
vadd_neon:
    stmdb            sp!, {r4, r5, sl, fp, lr}
@    vstmdb           sp!, {d8-d15}
                                        @ r0 = source1P
                                        @ r1 = source2P
                                        @ r2 = destP
                                        @ r3 = framesToProcess
    pld             [r0, #0]
    pld             [r1, #0]
                                        @ *source1P - q0,q1 - {d0-d3}
                                        @ *source2P - q2,q3 - {d4-d7}
                                        @ *destP    - q8,q9 - {d16-d19}
    asrs            r4, r3, #5          @ n = framesToProcess/32
    beq             ._vadd_process4

._vadd_do32:
    pld             [r0, #128]
    pld             [r1, #128]
    vld1.32         {d0-d3}, [r0]!      @ load *source1P; source1P+=8;
    vld1.32         {d4-d7}, [r1]!      @ load *source2P; source2P+=8;
    vadd.f32        q8, q2, q0          @ *destP = *source2P + *source1P;
    vadd.f32        q9, q3, q1          @ *destP = *source2P + *source1P;
    vst1.32         {d16-d19}, [r2]!    @ store *destP; destP += 8;

    vld1.32         {d0-d3}, [r0]!      @ load *source1P; source1P+=8;
    vld1.32         {d4-d7}, [r1]!      @ load *source2P; source2P+=8;
    vadd.f32        q8, q2, q0          @ *destP = *source2P + *source1P;
    vadd.f32        q9, q3, q1          @ *destP = *source2P + *source1P;
    vst1.32         {d16-d19}, [r2]!    @ store *destP; destP += 8;

    vld1.32         {d0-d3}, [r0]!      @ load *source1P; source1P+=8;
    vld1.32         {d4-d7}, [r1]!      @ load *source2P; source2P+=8;
    vadd.f32        q8, q2, q0          @ *destP = *source2P + *source1P;
    vadd.f32        q9, q3, q1          @ *destP = *source2P + *source1P;
    vst1.32         {d16-d19}, [r2]!    @ store *destP; destP += 8;

    vld1.32         {d0-d3}, [r0]!      @ load *source1P; source1P+=8;
    vld1.32         {d4-d7}, [r1]!      @ load *source2P; source2P+=8;
    vadd.f32        q8, q2, q0          @ *destP = *source2P + *source1P;
    vadd.f32        q9, q3, q1          @ *destP = *source2P + *source1P;
    vst1.32         {d16-d19}, [r2]!    @ store *destP; destP += 8;

    subs            r4, r4, #1          @ }while(--n);
    bne             ._vadd_do32

._vadd_process4:
    asr             r5, r3, #31
    lsr             r4, r5, #27
    add             r4, r4, r3
    ands            r4, r4, #31         @ if (framesToProcess % 32 == 0)
    beq             ._vadd_done

    asrs            r4, r4, #2          @ n = (framesToProcess % 32)/4
    beq             ._vadd_process1

._vadd_do4:
    vld1.32         {d0,d1}, [r0]!      @ load *source1P; source1P+=4;
    vld1.32         {d2,d3}, [r1]!      @ load *source2P; source2P+=4;
    vadd.f32        q2, q1, q0          @ *destP = *source2P + *source1P;
    vst1.32         {d4,d5}, [r2]!      @ store *destP; destP += 4;

    subs            r4, r4, #1          @ }while(--n);
    bne             ._vadd_do4

._vadd_process1:
    lsr             r4, r5, #30
    add             r4, r4, r3
    ands            r4, r4, #3          @ if (framesToProcess % 4 == 0)
    beq             ._vadd_done

._vadd_do1:
    vldr            s0, [r0]            @ load *source1P; source1P++;
    add             r0, r0, #4
    vldr            s2, [r1]            @ load *source2P; source2P++;
    add             r1, r1, #4
    vadd.f32        s1, s2, s0          @ *destP = *source2P + *source1P;
    vstr            s1, [r2]            @ store *destP; destP ++;
    add             r2, r2, #4

    subs            r4, r4, #1          @ }while(--n);
    bne             ._vadd_do1

._vadd_done:
@    vldmia          sp!, {d8-d15}
    ldmia           sp!, {r4, r5, sl, fp, pc}

    .endfunc
    .end
