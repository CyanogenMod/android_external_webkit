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

@void zvmul_neon(const float* real1P, const float* imag1P, const float* real2P, const float* imag2P, float* realDestP, float* imagDestP, size_t framesToProcess)

    .text
    .fpu neon
    .align 4
    .global     zvmul_neon
    .func       zvmul_neon
zvmul_neon:
    stmdb            sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    vstmdb           sp!, {d8-d15}
                                        @ r0 = real1P
                                        @ r1 = imag1P
                                        @ r2 = real2P
                                        @ r3 = imag2P
    pld             [r0, #0]
    pld             [r1, #0]
    pld             [r2, #0]
    pld             [r3, #0]
    ldr             r4, [sp, #100]      @ r4 = realDestP , Adjust sp when you store/restore registers
    ldr             r5, [sp, #104]      @ r5 = imagDestP
    ldr             r6, [sp, #108]      @ r6 = framesToProcess
                                        @ *real1P - q0,q1 - {d0-d3}
                                        @ *imag1P - q2,q3 - {d4-d7}
                                        @ *real2P - q4,q5 - {d8-d11}
                                        @ *imag2P - q6,q7 - {d12-d15}
                                        @ *realDestP - q8, q9 - {d16-d19}
                                        @ *imagDestP - q10, q11 - {d20-d23}
                                        @ tmp1 - q12, q13 - {d24-d27}
                                        @ tmp2 - q14, q15 - {d28-d31}
    asrs            r8, r6, #5          @ n = framesToProcess/32
    beq             ._vcpxmul_process4

._vcpxmul_do_while32:
    pld             [r0, #64]
    pld             [r1, #64]
    pld             [r2, #64]
    pld             [r3, #64]

    vld1.32         {d0-d3}, [r0]!      @ load *real1P; real1P += 8;
    vld1.32         {d4-d7}, [r1]!      @ load *imag1P; imag1P += 8;
    vld1.32         {d8-d11}, [r2]!     @ load *real2P; real2P += 8;
    vld1.32         {d12-d15}, [r3]!    @ load *imag2P; imag2P += 8;

    vmul.f32        q12, q0, q4         @ *real1P * *real2P
    vmul.f32        q13, q2, q6         @ *imag1P * *imag2P;
    vsub.f32        q8, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q0, q6         @ *real1P * *imag2P
    vmul.f32        q15, q2, q4         @ *imag1P * *real2P;
    vadd.f32        q10, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);

    vmul.f32        q12, q1, q5         @ *real1P * *real2P
    vmul.f32        q13, q3, q7         @ *imag1P * *imag2P;
    vsub.f32        q9, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q1, q7         @ *real1P * *imag2P
    vmul.f32        q15, q3, q5         @ *imag1P * *real2P;
    vadd.f32        q11, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);
    vst1.32         {d16-d19}, [r4]!    @ store *realDestP; realDestP += 8;
    vst1.32         {d20-d23}, [r5]!    @ store *imagDestP; imagDestP += 8;
@@@
    vld1.32         {d0-d3}, [r0]!      @ load *real1P; real1P += 8;
    vld1.32         {d4-d7}, [r1]!      @ load *imag1P; imag1P += 8;
    vld1.32         {d8-d11}, [r2]!     @ load *real2P; real2P += 8;
    vld1.32         {d12-d15}, [r3]!    @ load *imag2P; imag2P += 8;

    vmul.f32        q12, q0, q4         @ *real1P * *real2P
    vmul.f32        q13, q2, q6         @ *imag1P * *imag2P;
    vsub.f32        q8, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q0, q6         @ *real1P * *imag2P
    vmul.f32        q15, q2, q4         @ *imag1P * *real2P;
    vadd.f32        q10, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);

    vmul.f32        q12, q1, q5         @ *real1P * *real2P
    vmul.f32        q13, q3, q7         @ *imag1P * *imag2P;
    vsub.f32        q9, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q1, q7         @ *real1P * *imag2P
    vmul.f32        q15, q3, q5         @ *imag1P * *real2P;
    vadd.f32        q11, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);
    vst1.32         {d16-d19}, [r4]!    @ store *realDestP; realDestP += 8;
    vst1.32         {d20-d23}, [r5]!    @ store *imagDestP; imagDestP += 8;
@@@
    vld1.32         {d0-d3}, [r0]!      @ load *real1P; real1P += 8;
    vld1.32         {d4-d7}, [r1]!      @ load *imag1P; imag1P += 8;
    vld1.32         {d8-d11}, [r2]!     @ load *real2P; real2P += 8;
    vld1.32         {d12-d15}, [r3]!    @ load *imag2P; imag2P += 8;

    vmul.f32        q12, q0, q4         @ *real1P * *real2P
    vmul.f32        q13, q2, q6         @ *imag1P * *imag2P;
    vsub.f32        q8, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q0, q6         @ *real1P * *imag2P
    vmul.f32        q15, q2, q4         @ *imag1P * *real2P;
    vadd.f32        q10, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);

    vmul.f32        q12, q1, q5         @ *real1P * *real2P
    vmul.f32        q13, q3, q7         @ *imag1P * *imag2P;
    vsub.f32        q9, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q1, q7         @ *real1P * *imag2P
    vmul.f32        q15, q3, q5         @ *imag1P * *real2P;
    vadd.f32        q11, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);
    vst1.32         {d16-d19}, [r4]!    @ store *realDestP; realDestP += 8;
    vst1.32         {d20-d23}, [r5]!    @ store *imagDestP; imagDestP += 8;
@@@
    vld1.32         {d0-d3}, [r0]!      @ load *real1P; real1P += 8;
    vld1.32         {d4-d7}, [r1]!      @ load *imag1P; imag1P += 8;
    vld1.32         {d8-d11}, [r2]!     @ load *real2P; real2P += 8;
    vld1.32         {d12-d15}, [r3]!    @ load *imag2P; imag2P += 8;

    vmul.f32        q12, q0, q4         @ *real1P * *real2P
    vmul.f32        q13, q2, q6         @ *imag1P * *imag2P;
    vsub.f32        q8, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q0, q6         @ *real1P * *imag2P
    vmul.f32        q15, q2, q4         @ *imag1P * *real2P;
    vadd.f32        q10, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);

    vmul.f32        q12, q1, q5         @ *real1P * *real2P
    vmul.f32        q13, q3, q7         @ *imag1P * *imag2P;
    vsub.f32        q9, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q1, q7         @ *real1P * *imag2P
    vmul.f32        q15, q3, q5         @ *imag1P * *real2P;
    vadd.f32        q11, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);
    vst1.32         {d16-d19}, [r4]!    @ store *realDestP; realDestP += 8;
    vst1.32         {d20-d23}, [r5]!    @ store *imagDestP; imagDestP += 8;
@@@

    subs            r8, r8, #1          @ }while(--k);
    bne             ._vcpxmul_do_while32

._vcpxmul_process4:
    asr             r7, r6, #31
    lsr             r8, r7, #27
    add             r8, r8, r6
    ands            r8, r8, #31         @ if (framesToProcess % 32 == 0)
    beq             ._vcpxmul_done

    asrs            r8, r8, #2          @ n = (framesToProcess % 32)/4
    beq             ._vcpxmul_process1

._vcpxmul_do_while4:
    vld1.32         {d0,d1}, [r0]!      @load *real1P; real1P += 4;
    vld1.32         {d4,d5}, [r1]!      @load *imag1P; imag1P += 4;
    vld1.32         {d8,d9}, [r2]!     @load *real2P; real2P += 4;
    vld1.32         {d12,d13}, [r3]!    @load *imag2P; imag2P += 4;

    vmul.f32        q12, q0, q4         @ *real1P * *real2P
    vmul.f32        q13, q2, q6         @ *imag1P * *imag2P;
    vsub.f32        q8, q12, q13        @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        q14, q0, q6         @ *real1P * *imag2P
    vmul.f32        q15, q2, q4         @ *imag1P * *real2P;
    vadd.f32        q10, q14, q15       @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);

    vst1.32         {d16,d17}, [r4]!    @ store *realDestP; realDestP += 4;
    vst1.32         {d20,d21}, [r5]!    @ store *imagDestP; imagDestP += 4;

    subs            r8, r8, #1          @ }while(--k);
    bne             ._vcpxmul_do_while4

._vcpxmul_process1:
    lsr             r8, r7, #30
    add             r8, r8, r6
    ands            r8, r8, #3          @ if (framesToProcess % 4 == 0)
    beq             ._vcpxmul_done

._vcpxmul_do_while1:
    vldr            s0, [r0]            @load *real1P; real1P++;
    add             r0, r0, #4
    vldr            s1, [r1]            @load *imag1P; imag1P++;
    add             r1, r1, #4
    vldr            s2, [r2]            @load *real2P; real2P++;
    add             r2, r2, #4
    vldr            s3, [r3]            @load *imag2P; imag2P++;
    add             r3, r3, #4

    vmul.f32        s7, s0, s2          @ *real1P * *real2P
    vmul.f32        s8, s1, s3          @ *imag1P * *imag2P;
    vsub.f32        s4, s7, s8          @ realResult = (*real1P * *real2P) - (*imag1P * *imag2P);
    vmul.f32        s7, s0, s3          @ *real1P * *imag2P
    vmul.f32        s8, s1, s2          @ *imag1P * *real2P;
    vadd.f32        s5, s7, s8          @ imagResult= (*real1P * *imag2P) + (*imag1P * *real2P);

    vmul.f32        s4, s4, s9          @ realResult = scale * realResult;
    vstr            s4, [r4]            @ store *realDestP; realDestP ++;
    add             r4, r4, #4
    vmul.f32        s5, s5, s9          @ imagResult = scale * imagResult;
    vstr            s5, [r5]            @ store *imagDestP; imagDestP ++;
    add             r5, r5, #4

    subs            r8, r8, #1          @ }while(--k);
    bne             ._vcpxmul_do_while1

._vcpxmul_done:
    vldmia          sp!, {d8-d15}
    ldmia           sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
    nop

    .endfunc



