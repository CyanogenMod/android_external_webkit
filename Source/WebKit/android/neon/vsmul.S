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

@void vsmul_neon(const float* sourceP, const float* scale, float* destP, size_t framesToProcess)
    .text
    .fpu neon
    .align 4
    .global     vsmul_neon
    .func       vsmul_neon
vsmul_neon:
    stmdb            sp!, {r4, r5, sl, fp, lr}
    vstmdb           sp!, {d8, d9}
                                        @ r0 = sourceP
                                        @ r1 = scale
                                        @ r2 = destP
                                        @ r3 = framesToProcess
    pld             [r0, #0]
                                        @ *sourceP - q0, q1 - {d0-d3}
                                        @ *destP - q3, q4 - {d6-d9}
                                        @ scale  - q2 - {d4-d5} - s8-s11
    ldr             r5, [r1]            @ k = *scale;
    vdup.32         q2, r5
    asrs            r4, r3, #5          @ n = framesToProcess/32
    beq             ._vsmul_process4

._vsmul_do32:                           @ process 32 samples per iteration
    pld             [r0, #128]
    vld1.32         {d0-d3}, [r0]!      @ load *sourceP; sourceP+=8;
    vmul.f32        q3, q2, q0          @ *destP = k * *sourceP;
    vmul.f32        q4, q2, q1          @ *destP = k * *sourceP;
    vst1.32         {d6-d9}, [r2]!      @ store *destP; destP += 8;

    vld1.32         {d0-d3}, [r0]!      @ load *sourceP; sourceP+=8;
    vmul.f32        q3, q2, q0          @ *destP = k * *sourceP;
    vmul.f32        q4, q2, q1          @ *destP = k * *sourceP;
    vst1.32         {d6-d9}, [r2]!      @ store *destP; destP += 8;

    vld1.32         {d0-d3}, [r0]!      @ load *sourceP; sourceP+=8;
    vmul.f32        q3, q2, q0          @ *destP = k * *sourceP;
    vmul.f32        q4, q2, q1          @ *destP = k * *sourceP;
    vst1.32         {d6-d9}, [r2]!      @ store *destP; destP += 8;

    vld1.32         {d0-d3}, [r0]!      @ load *sourceP; sourceP+=8;
    vmul.f32        q3, q2, q0          @ *destP = k * *sourceP;
    vmul.f32        q4, q2, q1          @ *destP = k * *sourceP;
    vst1.32         {d6-d9}, [r2]!      @ store *destP; destP += 8;

    subs            r4, r4, #1          @ }while(--n);
    bne             ._vsmul_do32

._vsmul_process4:
    asr             r5, r3, #31
    lsr             r4, r5, #27
    add             r4, r4, r3
    ands            r4, r4, #31         @ if (framesToProcess % 32 == 0)
    beq             ._vsmul_done

    asrs            r4, r4, #2          @ n = (framesToProcess % 32)/4
    beq             ._vsmul_process1

._vsmul_do4:
    vld1.32         {d0,d1}, [r0]!      @ load *sourceP; sourceP+=4;
    vmul.f32        q1, q0, q2       @ *destP = k * *sourceP;
    vst1.32         {d2,d3}, [r2]!      @ store *destP; destP += 4;

    subs            r4, r4, #1          @ }while(--n);
    bne             ._vsmul_do4

._vsmul_process1:
    lsr             r4, r5, #30
    add             r4, r4, r3
    ands            r4, r4, #3          @ if (framesToProcess % 4 == 0)
    beq             ._vsmul_done

@    vldr            s2, [r1]            @ k = *scale;
._vsmul_do1:
    vldr            s0, [r0]            @ load *sourceP; sourceP+=4;
    add             r0, r0, #4
    vmul.f32        s1, s8, s0          @ *destP = k * *sourceP;
    vstr            s1, [r2]            @ store *destP; destP += 4;
    add             r2, r2, #4

    subs            r4, r4, #1          @ }while(--n);
    bne             ._vsmul_do1

._vsmul_done:
    vldmia          sp!, {d8, d9}
    ldmia           sp!, {r4, r5, sl, fp, pc}
    nop

    .endfunc
    .end
