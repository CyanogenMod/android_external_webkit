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

@void vintlve_neon(const float* realP, const float* imagP, float* destP, size_t framesToProcess)
    .text
    .fpu neon
    .align 4
    .global     vintlve_neon
    .func       vintlve_neon
vintlve_neon:
    stmdb            sp!, {r4, r5, sl, fp, lr}
@    vstmdb           sp!, {d8-d15}
                                        @ r0 = realP
                                        @ r1 = imagP
                                        @ r2 = destP
                                        @ r3 = framesToProcess
    pld             [r0, #0]
    pld             [r1, #0]
                                        @ *realP - q0 - {d0-d1}
                                        @ *imagP - q1 - {d2-d3}
                                        @ *destP  -q0,q1
    asrs            r4, r3, #5          @ n = framesToProcess/32
    beq             ._intlve_process4

._intlve_do32:
    pld             [r0, #64]
    pld             [r1, #64]
    vld1.32         {d0,d1}, [r0]!      @ load *realP; realP+=4;
    vld1.32         {d2,d3}, [r1]!      @ load *imagP; imagP+=4;
    vst2.32         {d0-d3}, [r2]!      @ store *destP; destP += 8;

    vld1.32         {d4,d5}, [r0]!      @ load *realP; realP+=4;
    vld1.32         {d6,d7}, [r1]!      @ load *imagP; imagP+=4;
    vst2.32         {d4-d7}, [r2]!      @ store *destP; destP += 8;

    vld1.32         {d0,d1}, [r0]!      @ load *realP; realP+=4;
    vld1.32         {d2,d3}, [r1]!      @ load *imagP; imagP+=4;
    vst2.32         {d0-d3}, [r2]!      @ store *destP; destP += 8;

    vld1.32         {d4,d5}, [r0]!      @ load *realP; realP+=4;
    vld1.32         {d6,d7}, [r1]!      @ load *imagP; imagP+=4;
    vst2.32         {d4-d7}, [r2]!      @ store *destP; destP += 8;

    subs            r4, r4, #1          @ }while(--n);
    bne             ._intlve_do32

._intlve_process4:
    asr             r5, r3, #31
    lsr             r4, r5, #27
    add             r4, r4, r3
    ands            r4, r4, #31         @ if (framesToProcess % 32 == 0)
    beq             ._intlve_done

    asrs            r4, r4, #2          @ n = (framesToProcess % 32)/4
    beq             ._intlve_do1

._intlve_do4:
    vld1.32         {d0}, [r0]!         @ load *realP; realP+=2;
    vld1.32         {d1}, [r1]!         @ load *imagP; imagP+=2;
    vst2.32         {d0,d1}, [r2]!      @ store *destP; destP += 4;

    subs            r4, r4, #1          @ }while(--n);
    bne             ._intlve_do4

@._intlve_process1:
    lsr             r4, r5, #30
    add             r4, r4, r3
    ands            r5, r4, #3          @ if (framesToProcess % 4 == 0)
    beq             ._intlve_done

._intlve_do1:
    vldr            s0, [r0]            @ load *realP; realP+=1;
    add             r0, r0, #4
    vldr            s1, [r1]            @ load *imagP; imagP+=1;
    add             r1, r1, #4
    vstr            d0, [r2]            @ store *destP; destP += 2;
    add             r2, r2, #8

    subs            r5, r5, #1          @ }while(--n);
    bne             ._intlve_do1

._intlve_done:
@    vldmia          sp!, {d8-d15}
    ldmia           sp!, {r4, r5, sl, fp, pc}
    nop

    .endfunc
    .end
