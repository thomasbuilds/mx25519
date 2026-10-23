/* Copyright (c) 2026 tevador <tevador@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*
 * ELF notes for an assembly source that assembles to nothing on this target.
 * They declare what an empty C file compiled with the same options declares:
 * a non-executable stack, and the control-flow protection (CET on x86; BTI,
 * PAC and GCS on AArch64) that the linker keeps only if every object has it.
 * A source that does assemble declares only what its own code implements.
 */

#if defined(__ELF__)
.section .note.GNU-stack,"",%progbits

#if (defined(__x86_64__) || defined(__i386__)) && defined(__CET__)
#define NOTE_TYPE 0xc0000002 /* GNU_PROPERTY_X86_FEATURE_1_AND */
#define NOTE_FEATURES __CET__
#elif defined(__aarch64__) && (defined(__ARM_FEATURE_BTI_DEFAULT) \
    || defined(__ARM_FEATURE_PAC_DEFAULT) || defined(__ARM_FEATURE_GCS_DEFAULT))
#define NOTE_TYPE 0xc0000000 /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
#if defined(__ARM_FEATURE_BTI_DEFAULT)
#define NOTE_BTI 1
#else
#define NOTE_BTI 0
#endif
#if defined(__ARM_FEATURE_PAC_DEFAULT)
#define NOTE_PAC 2
#else
#define NOTE_PAC 0
#endif
#if defined(__ARM_FEATURE_GCS_DEFAULT)
#define NOTE_GCS 4
#else
#define NOTE_GCS 0
#endif
#define NOTE_FEATURES (NOTE_BTI | NOTE_PAC | NOTE_GCS)
#endif

#if defined(NOTE_TYPE)
/* the property array is aligned to the pointer size */
#if defined(__LP64__)
#define NOTE_ALIGN 3
#else
#define NOTE_ALIGN 2
#endif
.section .note.gnu.property,"a"
.p2align NOTE_ALIGN
.long 1f - 0f
.long 4f - 1f
.long 5
0: .asciz "GNU"
1: .p2align NOTE_ALIGN
.long NOTE_TYPE
.long 3f - 2f
2: .long NOTE_FEATURES
3: .p2align NOTE_ALIGN
4:
#endif
#endif
