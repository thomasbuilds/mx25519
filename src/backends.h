/* Copyright (c) 2022-2026 tevador <tevador@gmail.com>
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

#ifndef BACKENDS_H
#define BACKENDS_H

/*
 * Decides which assembly backend the target can use. Preprocessor only, so
 * the GNU assembly sources include it too and assemble to nothing on any
 * target the C code would not call them on.
 *
 * MX25519_NO_ASM_AMD64 and MX25519_NO_ASM_ARM64 tell the C code that the
 * build did not include that backend's assembly source.
 */

#if !defined(MX25519_NO_COPYLEFT)
/* x86-64 with 64-bit pointers: not x32, not ARM64EC */
#if ((defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64)) \
    && !defined(_M_ARM64EC) && !defined(MX25519_NO_ASM_AMD64)
#define IMPL_AMD64
/* little-endian AArch64 with 64-bit pointers, GNU-style assembler only */
#elif defined(__aarch64__) && defined(__AARCH64EL__) && !defined(__ILP32__) \
    && !defined(_MSC_VER) && !defined(MX25519_NO_ASM_ARM64)
#define IMPL_ARM64
#endif
#endif

#endif
