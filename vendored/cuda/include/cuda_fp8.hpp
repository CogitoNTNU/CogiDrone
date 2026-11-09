/*
 * Copyright 2022-2026 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#if !defined(__CUDA_FP8_HPP__)
#define __CUDA_FP8_HPP__

#if !defined(__CUDA_FP8_H__)
#error "Do not include this file directly. Instead, include cuda_fp8.h."
#endif

/* C++ header for ::std::memcpy (used for type punning in host-side
 * implementations). When compiling as a CUDA source file memcpy is provided
 * implicitly. !defined(__CUDACC__) implies !defined(__CUDACC_RTC__).
 */
#if defined(__cplusplus) && !defined(__CUDACC__)
#include <cstring>
#elif !defined(__cplusplus) && !defined(__CUDACC__)
#include <string.h>
#endif /* defined(__cplusplus) && !defined(__CUDACC__) */

// implicitly provided by NVRTC
#if !defined(__CUDACC_RTC__)
#include <nv/target>
#endif  /* !defined(__CUDACC_RTC__) */

#if !defined(IF_DEVICE_OR_CUDACC)
#if defined(__CUDACC__)
    #define IF_DEVICE_OR_CUDACC(d, c, f) NV_IF_ELSE_TARGET(NV_IS_DEVICE, d, c)
#else
    #define IF_DEVICE_OR_CUDACC(d, c, f) NV_IF_ELSE_TARGET(NV_IS_DEVICE, d, f)
#endif
#endif

/* Set up structure-alignment attribute */
#if !(defined __CUDA_ALIGN__)
#if defined(__CUDACC__)
#define __CUDA_ALIGN__(align) __align__(align)
#else
/* Define alignment macro based on compiler type (cannot assume C11 "_Alignas"
 * is available) */
#if __cplusplus >= 201103L
#define __CUDA_ALIGN__(n)                                                      \
    alignas(n) /* C++11 kindly gives us a keyword for this */
#else          /* !defined(__CPP_VERSION_AT_LEAST_11_FP8)*/
#if defined(__GNUC__)
#define __CUDA_ALIGN__(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define __CUDA_ALIGN__(n) __declspec(align(n))
#else
#define __CUDA_ALIGN__(n)
#endif /* defined(__GNUC__) */
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */
#endif /* defined(__CUDACC__) */
#endif /* !(defined __CUDA_ALIGN__) */

#if !(defined __CPP_VERSION_AT_LEAST_11_FP8)
/* need c++11 for explicit operators */
#define __CUDA_NO_FP8_CONVERSION_OPERATORS__
#endif

#if !(defined __DOXYGEN_ONLY__)

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_saturation_t
__internal_fp8_sanitize_saturation(const __nv_saturation_t saturate) {
    return (saturate == __NV_NOSAT) ? __NV_NOSAT : __NV_SATFINITE;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8_interpretation_t
__internal_fp8_sanitize_interpretation(
    const __nv_fp8_interpretation_t fp8_interpretation) {
    return (fp8_interpretation == __NV_E5M2) ? __NV_E5M2 : __NV_E4M3;
}

__CUDA_HOSTDEVICE_FP8_DECL__ enum cudaRoundMode
__internal_fp8_sanitize_rounding(const enum cudaRoundMode rounding) {
    enum cudaRoundMode sanitized_rounding;
    switch (rounding) {
        case cudaRoundZero:
        case cudaRoundPosInf:
        case cudaRoundMinInf:
            sanitized_rounding = rounding;
            break;
        case cudaRoundNearest:
        default:
            sanitized_rounding = cudaRoundNearest;
            break;
    }
    return sanitized_rounding;
}

__CUDA_HOSTDEVICE_FP8_DECL__ enum cudaRoundMode
__internal_fp8_sanitize_e8m0_rounding(const enum cudaRoundMode rounding) {
    enum cudaRoundMode sanitized_rounding;
    switch (rounding) {
        case cudaRoundNearest:
            sanitized_rounding = cudaRoundNearest;
            break;
        case cudaRoundZero:
        case cudaRoundMinInf:
            sanitized_rounding = cudaRoundZero;
            break;
        case cudaRoundPosInf:
        default:
            sanitized_rounding = cudaRoundPosInf;
            break;
    }
    return sanitized_rounding;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8_storage_t
__nv_cvt_double_to_fp8(const double x, const __nv_saturation_t saturate,
                       const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    unsigned char res;
    unsigned long long int xbits;

#if defined(__CUDACC__) || (!defined __cplusplus)
    (void)memcpy(&xbits, &x, sizeof(x));
#else
    (void)::std::memcpy(&xbits, &x, sizeof(x));
#endif
    unsigned char FP8_MAXNORM;
    unsigned char FP8_MANTISSA_MASK;
    unsigned short int FP8_EXP_BIAS;
    unsigned long long int FP8_SIGNIFICAND_BITS;
    const unsigned long long int DP_INF_BITS = 0x7FF0000000000000ULL;
    unsigned long long int FP8_MINDENORM_O2;
    unsigned long long int FP8_OVERFLOW_THRESHOLD;
    unsigned long long int FP8_MINNORM;

    if (sanitized_fp8_interpretation == __NV_E4M3) {
        FP8_EXP_BIAS = 7U;
        FP8_SIGNIFICAND_BITS = 4ULL;
        FP8_MANTISSA_MASK = 0x7U;
        FP8_MINDENORM_O2 = 0x3F50000000000000ULL; // mindenorm/2 = 2^-10
        FP8_OVERFLOW_THRESHOLD =
            0x407D000000000000ULL; // maxnorm + 1/2ulp = 0x1.Cp+8 + 0x1p+4
        FP8_MAXNORM = 0x7EU;
        FP8_MINNORM = 0x3F90000000000000ULL; // minnorm = 2^-6
    } else {                                 //__NV_E5M2
        FP8_EXP_BIAS = 15U;
        FP8_SIGNIFICAND_BITS = 3ULL;
        FP8_MANTISSA_MASK = 0x3U;
        FP8_MINDENORM_O2 = 0x3EE0000000000000ULL; // mindenorm/2 = 2^-17
        FP8_OVERFLOW_THRESHOLD =
            0x40EE000000000000ULL -
            1ULL; // maxnorm + 1/2ulp = 0x1.Ep+15, and -1 to have common code
        FP8_MAXNORM = 0x7BU;
        FP8_MINNORM = 0x3F10000000000000ULL; // minnorm = 2^-14
    }

    // 1/2 LSB of the target format, positioned in double precision mantissa
    // helpful for midpoints detection during round-to-nearest-even step
    const unsigned long long int FP8_DP_HALF_ULP =
        (unsigned long long int)1ULL << (53ULL - FP8_SIGNIFICAND_BITS - 1ULL);
    // prepare sign bit in target format
    unsigned char sign = (unsigned char)((xbits >> 63ULL) << 7U);
    // prepare exponent field in target format
    unsigned char exp =
        (unsigned char)((((unsigned short int)(xbits >> 52ULL)) & 0x7FFU) -
                        1023U + FP8_EXP_BIAS);
    // round mantissa to target format width, rounding towards zero
    unsigned char mantissa =
        (unsigned char)(xbits >> (53ULL - FP8_SIGNIFICAND_BITS)) &
        FP8_MANTISSA_MASK;
    unsigned long long int absx = xbits & 0x7FFFFFFFFFFFFFFFULL;

    if (absx <= FP8_MINDENORM_O2) {
        // zero or underflow
        res = 0U;
    } else if (absx > DP_INF_BITS) {
        // NaN
        if (sanitized_fp8_interpretation == __NV_E4M3) {
            res = 0x7FU;
        } else {
            // NaN --> QNaN
            res = 0x7EU | mantissa;
        }
    } else if (absx > FP8_OVERFLOW_THRESHOLD) {
        if (sanitized_saturate == __NV_SATFINITE) {
            res = FP8_MAXNORM;
        } else {
            // __NV_NOSAT
            if (sanitized_fp8_interpretation == __NV_E4M3) {
                // no Inf in E4M3
                res = 0x7FU; // NaN
            } else {
                res = 0x7CU; // Inf in E5M2
            }
        }
    } else if (absx >= FP8_MINNORM) {
        res = (unsigned char)((exp << (FP8_SIGNIFICAND_BITS - 1U)) | mantissa);
        // rounded-off bits
        unsigned long long int round =
            xbits & ((FP8_DP_HALF_ULP << 1ULL) - 1ULL);
        // round-to-nearest-even adjustment
        if ((round > FP8_DP_HALF_ULP) ||
            ((round == FP8_DP_HALF_ULP) && (mantissa & 1U))) {
            res = (unsigned char)(res + 1U);
        }
    } else // Denormal range
    {
        unsigned char shift = (unsigned char)(1U - exp);
        // add implicit leading bit
        mantissa |= (unsigned char)(1U << (FP8_SIGNIFICAND_BITS - 1U));
        // additional round-off due to denormalization
        res = (unsigned char)(mantissa >> shift);

        // rounded-off bits, including implicit leading bit
        unsigned long long int round =
            (xbits | ((unsigned long long int)1ULL << (53ULL - 1ULL))) &
            ((FP8_DP_HALF_ULP << (shift + 1ULL)) - 1ULL);
        // round-to-nearest-even adjustment
        if ((round > (FP8_DP_HALF_ULP << shift)) ||
            ((round == (FP8_DP_HALF_ULP << shift)) && (res & 1U))) {
            res = (unsigned char)(res + 1U);
        }
    }

    res |= sign;

    return (__nv_fp8_storage_t)res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8x2_storage_t
__nv_cvt_double2_to_fp8x2(
    const double2 x, const __nv_saturation_t saturate,
    const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_fp8x2_storage_t storage = (__nv_fp8x2_storage_t)__nv_cvt_double_to_fp8(
        x.y, sanitized_saturate, sanitized_fp8_interpretation);
    storage = (__nv_fp8x2_storage_t)(storage << 8U);
    storage = (__nv_fp8x2_storage_t)(storage |
                                     __nv_cvt_double_to_fp8(
                                         x.x, sanitized_saturate,
                                         sanitized_fp8_interpretation));
    return storage;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8_storage_t
__nv_cvt_float_to_fp8(const float x, const __nv_saturation_t saturate,
                      const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_fp8_storage_t res = 0U;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 890)
    if (sanitized_saturate == __NV_SATFINITE) {
        __nv_fp8x2_storage_t storage;
        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.satfinite.e5m2x2.f32 %0, %2, %1;}\n"
                : "=h"(storage)
                : "f"(x), "f"(0.0f));
        } else {
            asm("{cvt.rn.satfinite.e4m3x2.f32 %0, %2, %1;}\n"
                : "=h"(storage)
                : "f"(x), "f"(0.0f));
        }
        res = (__nv_fp8_storage_t)storage;
    } else
#endif
    {
        unsigned int xbits;
#if defined(__CUDACC__) || (!defined __cplusplus)
        (void)memcpy(&xbits, &x, sizeof(x));
#else
        (void)::std::memcpy(&xbits, &x, sizeof(x));
#endif

        // isnan
        if ((xbits & 0x7FFFFFFFU) > 0x7F800000U) {
            // Canonical NaN
            xbits = 0x7FFFFFFFU;
        }

        float fx;
#if defined(__CUDACC__) || (!defined __cplusplus)
        (void)memcpy(&fx, &xbits, sizeof(xbits));
#else
        (void)::std::memcpy(&fx, &xbits, sizeof(xbits));
#endif

        const double dx = (double)fx;
        res = __nv_cvt_double_to_fp8(
            dx, sanitized_saturate, sanitized_fp8_interpretation);
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8x2_storage_t
__nv_cvt_float2_to_fp8x2(
    const float2 x, const __nv_saturation_t saturate,
    const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_fp8x2_storage_t storage;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 890)
    if (sanitized_saturate == __NV_SATFINITE) {
        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.satfinite.e5m2x2.f32 %0, %2, %1;}\n"
                : "=h"(storage)
                : "f"(x.x), "f"(x.y));
        } else {
            asm("{cvt.rn.satfinite.e4m3x2.f32 %0, %2, %1;}\n"
                : "=h"(storage)
                : "f"(x.x), "f"(x.y));
        }
    } else
#endif
    {
        storage = (__nv_fp8x2_storage_t)__nv_cvt_float_to_fp8(
            x.y, sanitized_saturate, sanitized_fp8_interpretation);
        storage = (__nv_fp8x2_storage_t)(storage << 8U);
        storage = (__nv_fp8x2_storage_t)(storage | __nv_cvt_float_to_fp8(
                                                       x.x, sanitized_saturate,
                                                       sanitized_fp8_interpretation));
    }
    return storage;
}

__CUDA_HOSTDEVICE_FP8_DECL__ float
__internal_halfraw_to_float(const __half_raw x) {
    float f;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 530)
    asm("{cvt.f32.f16 %0, %1;}\n" : "=f"(f) : "h"(x.x));
#else
    const unsigned int ux = (unsigned int)x.x;
    unsigned int sign = (ux >> 15U) & 1U;
    unsigned int exponent = (ux >> 10U) & 0x1fU;
    unsigned int mantissa = (ux & 0x3ffU) << 13U;
    if (exponent == 0x1fU) { /* NaN or Inf */
        /* discard sign of a NaN */
        sign = ((mantissa != 0U) ? (sign >> 1U) : sign);
        mantissa = ((mantissa != 0U) ? 0x7fffffU : 0U);
        exponent = 0xffU;
    } else if (exponent == 0U) { /* Denorm or Zero */
        if (mantissa != 0U) {
            unsigned int msb;
            exponent = 0x71U;
            do {
                msb = (mantissa & 0x400000U);
                mantissa <<= 1U; /* normalize */
                --exponent;
            } while (msb == 0U);
            mantissa &= 0x7fffffU; /* 1.mantissa is implicit */
        }
    } else {
        exponent += 0x70U;
    }
    const unsigned int u = ((sign << 31U) | (exponent << 23U) | mantissa);
#if defined(__CUDACC__) || (!defined __cplusplus)
    (void)memcpy(&f, &u, sizeof(u));
#else
    (void)::std::memcpy(&f, &u, sizeof(u));
#endif
#endif /* (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 530) */
    return f;
}

__CUDA_HOSTDEVICE_FP8_DECL__ float2
__internal_halfraw2_to_float2(const __half2_raw x) {
    __half_raw raw;
    float2 res;
    raw.x = x.x;
    res.x = __internal_halfraw_to_float(raw);
    raw.x = x.y;
    res.y = __internal_halfraw_to_float(raw);
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8_storage_t
__nv_cvt_halfraw_to_fp8(
    const __half_raw x, const __nv_saturation_t saturate,
    const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_fp8_storage_t res = 0U;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 890)
    if (sanitized_saturate == __NV_SATFINITE) {
        unsigned int half2_storage = (unsigned int)(x.x);
        __nv_fp8x2_storage_t tmp;
        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.satfinite.e5m2x2.f16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(half2_storage));
        } else {
            asm("{cvt.rn.satfinite.e4m3x2.f16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(half2_storage));
        }
        res = (__nv_fp8_storage_t)tmp;
    } else
#endif
    {
        float fx = __internal_halfraw_to_float(x);
        res = __nv_cvt_float_to_fp8(
            fx, sanitized_saturate, sanitized_fp8_interpretation);
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8x2_storage_t __nv_cvt_halfraw2_to_fp8x2(
    const __half2_raw x, const __nv_saturation_t saturate,
    const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_fp8x2_storage_t tmp;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 890)
    if (sanitized_saturate == __NV_SATFINITE) {
        unsigned int half2_storage;
        (void)memcpy(&half2_storage, &x, sizeof(x));

        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.satfinite.e5m2x2.f16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(half2_storage));
        } else {
            asm("{cvt.rn.satfinite.e4m3x2.f16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(half2_storage));
        }
    } else
#endif
    {
        __half_raw raw;
        raw.x = x.x;
        __nv_fp8_storage_t lo =
            __nv_cvt_halfraw_to_fp8(
                raw, sanitized_saturate, sanitized_fp8_interpretation);
        raw.x = x.y;
        __nv_fp8_storage_t hi =
            __nv_cvt_halfraw_to_fp8(
                raw, sanitized_saturate, sanitized_fp8_interpretation);
        tmp = hi;
        tmp = (__nv_fp8x2_storage_t)(tmp << 8U);
        tmp = (__nv_fp8x2_storage_t)(tmp | lo);
    }
    return tmp;
}

__CUDA_HOSTDEVICE_FP8_DECL__ float
__internal_bf16raw_to_float(const __nv_bfloat16_raw x) {
    const unsigned int ux = ((unsigned int)x.x) << 16U;
    float fx;
#if defined(__CUDACC__) || (!defined __cplusplus)
    (void)memcpy(&fx, &ux, sizeof(ux));
#else
    (void)::std::memcpy(&fx, &ux, sizeof(ux));
#endif
    return fx;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat16_raw
__internal_float_to_bf16raw_rz(const float x);

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat16_raw
__internal_float_to_bf16raw_ru(const float x);

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8_storage_t __nv_cvt_bfloat16raw_to_fp8(
    const __nv_bfloat16_raw x, const __nv_saturation_t saturate,
    const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);

    __nv_fp8_storage_t res = 0U;
#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    if (sanitized_saturate == __NV_SATFINITE) {
        unsigned int bf162_storage = (unsigned int)(x.x);
        __nv_fp8x2_storage_t tmp;
        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.satfinite.e5m2x2.bf16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(bf162_storage));
        } else {
            asm("{cvt.rn.satfinite.e4m3x2.bf16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(bf162_storage));
        }
        res = (__nv_fp8_storage_t)tmp;
    } else
#endif
    {
        const float fx = __internal_bf16raw_to_float(x);
        res = __nv_cvt_float_to_fp8(
            fx, sanitized_saturate, sanitized_fp8_interpretation);
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_fp8x2_storage_t
__nv_cvt_bfloat16raw2_to_fp8x2(
    const __nv_bfloat162_raw x, const __nv_saturation_t saturate,
    const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_fp8x2_storage_t tmp;
#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    if (sanitized_saturate == __NV_SATFINITE) {
        unsigned int bf162_storage;
        (void)memcpy(&bf162_storage, &x, sizeof(x));

        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.satfinite.e5m2x2.bf16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(bf162_storage));
        } else {
            asm("{cvt.rn.satfinite.e4m3x2.bf16x2 %0, %1;}\n"
                : "=h"(tmp)
                : "r"(bf162_storage));
        }
    } else
#endif
    {
        __nv_bfloat16_raw raw;
        raw.x = x.x;
        __nv_fp8_storage_t lo =
            __nv_cvt_bfloat16raw_to_fp8(
                raw, sanitized_saturate, sanitized_fp8_interpretation);
        raw.x = x.y;
        __nv_fp8_storage_t hi =
            __nv_cvt_bfloat16raw_to_fp8(
                raw, sanitized_saturate, sanitized_fp8_interpretation);
        tmp = hi;
        tmp = (__nv_fp8x2_storage_t)(tmp << 8U);
        tmp = (__nv_fp8x2_storage_t)(tmp | lo);
    }
    return tmp;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __half2_raw
__nv_cvt_fp8x2_to_halfraw2(const __nv_fp8x2_storage_t x,
                           const __nv_fp8_interpretation_t fp8_interpretation);

__CUDA_HOSTDEVICE_FP8_DECL__ __half_raw
__nv_cvt_fp8_to_halfraw(const __nv_fp8_storage_t x,
                        const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __half_raw res;
    res.x = 0U;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 890)
    res.x =
        __nv_cvt_fp8x2_to_halfraw2(
            (__nv_fp8x2_storage_t)x, sanitized_fp8_interpretation)
            .x;
#else
    unsigned short int ur = (unsigned short int)x;
    ur = (unsigned short int)(ur << 8U);

    if (sanitized_fp8_interpretation == __NV_E5M2) {
        if ((ur & 0x7FFFU) > 0x7C00U) {
            /* If NaN, return canonical NaN */
            ur = 0x7FFFU;
        }
    } else { // __NV_E4M3
        unsigned short int sign = ur & 0x8000U;
        unsigned short int exponent =
            (unsigned short int)(((ur & 0x7800U) >> 1U) + 0x2000U);
        unsigned short int mantissa = (ur & 0x0700U) >> 1U;
        unsigned char absx = 0x7FU & (unsigned char)x;

        if (absx == 0x7FU) // NaN
        {
            ur = 0x7FFFU; // fp16 canonical NaN, discard sign
        } else if (exponent == 0x2000U) {
            // zero or denormal
            if (mantissa != 0U) {
                // normalize
                mantissa = (unsigned short int)(mantissa << 1U);
                while ((mantissa & 0x0400U) == 0U) {
                    mantissa = (unsigned short int)(mantissa << 1U);
                    exponent = (unsigned short int)(exponent - 0x0400U);
                }
                // discard implicit leading bit
                mantissa &= 0x03FFU;
            } else { // Zero
                exponent = 0U;
            }

            ur = (sign | exponent) | mantissa;
        } else {
            ur = (sign | exponent) | mantissa;
        }
    }
    res.x = ur;
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __half2_raw
__nv_cvt_fp8x2_to_halfraw2(const __nv_fp8x2_storage_t x,
                           const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __half2_raw res;
#if (defined __CUDA_ARCH__) && (__CUDA_ARCH__ >= 890)
    unsigned int half2_storage;
    if (sanitized_fp8_interpretation == __NV_E5M2) {
        asm("{cvt.rn.f16x2.e5m2x2 %0, %1;}\n" : "=r"(half2_storage) : "h"(x));
    } else {
        asm("{cvt.rn.f16x2.e4m3x2 %0, %1;}\n" : "=r"(half2_storage) : "h"(x));
    }
    (void)memcpy(&res, &half2_storage, sizeof(half2_storage));
#else
    res.x =
        __nv_cvt_fp8_to_halfraw(
            (__nv_fp8_storage_t)x, sanitized_fp8_interpretation).x;
    res.y = __nv_cvt_fp8_to_halfraw((__nv_fp8_storage_t)(x >> 8U),
                                    sanitized_fp8_interpretation)
                .x;
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat16_raw
__nv_cvt_fp8_to_bf16raw(const __nv_fp8_storage_t x,
                        const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_bfloat16_raw res;
    #if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
        res.x = __nv_cvt_fp8x2_to_bf162raw(
            (__nv_fp8x2_storage_t)x, sanitized_fp8_interpretation).x;
    #else
        res = __internal_float_to_bf16raw_rz(
            __internal_halfraw_to_float(
                __nv_cvt_fp8_to_halfraw(x, sanitized_fp8_interpretation)));
    #endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat162_raw
__nv_cvt_fp8x2_to_bf162raw(const __nv_fp8x2_storage_t x,
                           const __nv_fp8_interpretation_t fp8_interpretation) {
    const __nv_fp8_interpretation_t sanitized_fp8_interpretation =
        __internal_fp8_sanitize_interpretation(fp8_interpretation);
    __nv_bfloat162_raw res;
    #if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
        unsigned int bf162_storage;
        if (sanitized_fp8_interpretation == __NV_E5M2) {
            asm("{cvt.rn.bf16x2.e5m2x2 %0, %1;}\n"
                : "=r"(bf162_storage)
                : "h"(x));
        } else {
            asm("{cvt.rn.bf16x2.e4m3x2 %0, %1;}\n"
                : "=r"(bf162_storage)
                : "h"(x));
        }
        (void)memcpy(&res, &bf162_storage, sizeof(bf162_storage));
    #else
        res.x = __nv_cvt_fp8_to_bf16raw(
            (__nv_fp8_storage_t)x, sanitized_fp8_interpretation).x;
        res.y = __nv_cvt_fp8_to_bf16raw((__nv_fp8_storage_t)(x >> 8U),
                                    sanitized_fp8_interpretation).x;
    #endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __internal_cvt_bfloat16raw_to_e8m0_rn(
    const __nv_bfloat16_raw x, const __nv_saturation_t saturate) {
    const unsigned short int absx = x.x & 0x7FFFU;
    const unsigned short int BF16_INF_BITS = 0x7F80U;
    __nv_fp8_storage_t res;

    if (absx > BF16_INF_BITS) {
        res = (__nv_fp8_storage_t)0xFFU;
    } else {
        if (absx < 0x0040U) {
            /* Results below 2^-127 are clipped to the minimum E8M0 value. */
            res = (__nv_fp8_storage_t)0U;
        } else if (absx < 0x0080U) {
            /* The midpoint between 2^-127 and 2^-126 is BF16 0x0060. */
            res = (absx >= 0x0060U) ? (__nv_fp8_storage_t)1U
                                    : (__nv_fp8_storage_t)0U;
        } else {
            res = (__nv_fp8_storage_t)(absx >> 7U);
            if ((absx < BF16_INF_BITS) &&
                ((absx & 0x007FU) >= 0x0040U)) {
                /* E8M0 nearest rounding sends an exact midpoint upward. */
                res = (__nv_fp8_storage_t)(res + 1U);
            }
        }

        if ((saturate == __NV_SATFINITE) && (res == 0xFFU)) {
            res = (__nv_fp8_storage_t)0xFEU;
        }
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_bfloat16raw_to_e8m0(const __nv_bfloat16_raw x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_e8m0_rounding(rounding);
    __nv_fp8_storage_t res;

#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short ures = 0U;
    unsigned in = (unsigned)(x.x);
    if (sanitized_rounding == cudaRoundNearest) {
        ures = (unsigned short)__internal_cvt_bfloat16raw_to_e8m0_rn(
            x, sanitized_saturate);
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    }
    res = (__nv_fp8_storage_t)ures;
#else
    // extract exponent bits, provides non-saturated result in RZ
    res = (sanitized_rounding == cudaRoundNearest) ?
        __internal_cvt_bfloat16raw_to_e8m0_rn(x, sanitized_saturate) :
        (unsigned char)(x.x >> 7U);

    if (sanitized_rounding == cudaRoundPosInf) {
        // round-up if mantissa non-zero and |x| > 2^-127 and finite
        if ((x.x & 0x007FU) && ((x.x & 0x7FFFU) > 0x0040U) && ((x.x & 0x7FFFU) < 0x7F80U)) res++;
    }

    // Handle saturation of non-NaN large inputs to finite
    if (sanitized_saturate == __NV_SATFINITE) {
        // non-NaN, Overflow --> Max
        if (((x.x & 0x7FFFU) <= 0x7F80U) && (res == 0xFFU))
        {
            res--;
        }
    }
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_bfloat162raw_to_e8m0x2(const __nv_bfloat162_raw x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_e8m0_rounding(rounding);
    __nv_fp8x2_storage_t res;

#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    res = 0U;
    unsigned in = (unsigned)(x.x) | ((unsigned)(x.y) << (unsigned)16U);
    if (sanitized_rounding == cudaRoundNearest) {
        __nv_bfloat16_raw lo, hi;
        lo.x = x.x;
        hi.x = x.y;
        res = __internal_cvt_bfloat16raw_to_e8m0_rn(
            hi, sanitized_saturate);
        res = (__nv_fp8x2_storage_t)(res << 8U);
        res = (__nv_fp8x2_storage_t)(
            res | __internal_cvt_bfloat16raw_to_e8m0_rn(
                      lo, sanitized_saturate));
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(res)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(res)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(res)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue8m0x2.bf16x2 %0, %1;}\n"
                : "=h"(res)
                : "r"(in));
    }
#else
    __nv_bfloat16_raw lo, hi;
    lo.x = x.x;
    hi.x = x.y;
    res = __nv_cvt_bfloat16raw_to_e8m0(
        hi, sanitized_saturate, sanitized_rounding);
    res <<= (unsigned short)8U;
    res |= __nv_cvt_bfloat16raw_to_e8m0(
        lo, sanitized_saturate, sanitized_rounding);
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ unsigned int __internal_fp8_float_as_uint(const float f)
{
    unsigned int u;
#if defined(__CUDACC__) || (!defined __cplusplus)
    (void)memcpy(&u, &f, sizeof(f));
#else
    (void)::std::memcpy(&u, &f, sizeof(f));
#endif
    return u;
}

__CUDA_HOSTDEVICE_FP8_DECL__ float __internal_fp8_uint_as_float(const unsigned int u)
{
    float f;
#if defined(__CUDACC__) || (!defined __cplusplus)
    (void)memcpy(&f, &u, sizeof(u));
#else
    (void)::std::memcpy(&f, &u, sizeof(u));
#endif
    return f;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __internal_cvt_float_to_e8m0_rn(
    const float x, const __nv_saturation_t saturate) {
    const unsigned int absx = __internal_fp8_float_as_uint(x) & 0x7FFFFFFFU;
    const unsigned int FP32_INF_BITS = 0x7F800000U;
    __nv_fp8_storage_t res;

    if (absx > FP32_INF_BITS) {
        res = (__nv_fp8_storage_t)0xFFU;
    } else {
        if (absx < 0x00400000U) {
            /* Results below 2^-127 are clipped to the minimum E8M0 value. */
            res = (__nv_fp8_storage_t)0U;
        } else if (absx < 0x00800000U) {
            /* The midpoint between 2^-127 and 2^-126 is FP32 0x00600000. */
            res = (absx >= 0x00600000U) ? (__nv_fp8_storage_t)1U
                                        : (__nv_fp8_storage_t)0U;
        } else {
            res = (__nv_fp8_storage_t)(absx >> 23U);
            if ((absx < FP32_INF_BITS) &&
                ((absx & 0x007FFFFFU) >= 0x00400000U)) {
                /* E8M0 nearest rounding sends an exact midpoint upward. */
                res = (__nv_fp8_storage_t)(res + 1U);
            }
        }

        if ((saturate == __NV_SATFINITE) && (res == 0xFFU)) {
            res = (__nv_fp8_storage_t)0xFEU;
        }
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat16_raw
__internal_float_to_bf16raw_rz(const float x) {
    __nv_bfloat16_raw r;
NV_IF_ELSE_TARGET(NV_PROVIDES_SM_80,
    asm("{  cvt.rz.bf16.f32 %0, %1;}\n" : "=h"(r.x) : "f"(x));
,
    unsigned int ux = __internal_fp8_float_as_uint(x);
    if ((ux & 0x7FFFFFFFU) > 0x7f800000U)
    {
        // NaN
        r.x = (unsigned short int)0x7FFFU;
    }
    else
    {
        r.x = (unsigned short int)(ux >> 16U);
    }
)
    return r;
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat16_raw
__internal_float_to_bf16raw_ru(const float x) {
    __nv_bfloat16_raw r;
NV_IF_ELSE_TARGET(NV_PROVIDES_SM_90,
    asm("{  cvt.rp.bf16.f32 %0, %1;}\n" : "=h"(r.x) : "f"(x));
,
    unsigned int ux = __internal_fp8_float_as_uint(x);
    if ((ux & 0x7FFFFFFFU) > 0x7f800000U)
    {
        // NaN
        r.x = (unsigned short int)0x7FFFU;
    }
    else if ((ux < 0x7f800000U) && ((ux & 0x0000FFFFU) != 0))
    {
        // 0 <= x < +inf, round-up
        r.x = (unsigned short int)((ux >> 16U) + 1U);
    }
    else {
        // truncate others
        r.x = (unsigned short int)(ux >> 16U);
    }
)
    return r;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_float_to_e8m0(const float x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_e8m0_rounding(rounding);
    __nv_fp8_storage_t res = 0U;
#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short ures = 0U;
    if (sanitized_rounding == cudaRoundNearest) {
        ures = (unsigned short)__internal_cvt_float_to_e8m0_rn(
            x, sanitized_saturate);
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    }
    res = (__nv_fp8_storage_t)ures;
#else
    if (sanitized_rounding == cudaRoundNearest)
    {
        res = __internal_cvt_float_to_e8m0_rn(x, sanitized_saturate);
    }
    else if (sanitized_rounding == cudaRoundZero)
    {
        res = __nv_cvt_bfloat16raw_to_e8m0(
            __internal_float_to_bf16raw_rz(x), sanitized_saturate,
            sanitized_rounding);
    }
    else
    {   //cudaRoundPosInf
        float absx = __internal_fp8_uint_as_float((__internal_fp8_float_as_uint(x) << (unsigned)1U) >> (unsigned)1U);
        res = __nv_cvt_bfloat16raw_to_e8m0(
            __internal_float_to_bf16raw_ru(absx), sanitized_saturate,
            sanitized_rounding);
    }
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_float2_to_e8m0x2(const float2 x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_e8m0_rounding(rounding);
    __nv_fp8x2_storage_t res = 0U;

#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    if (sanitized_rounding == cudaRoundNearest) {
        res = __internal_cvt_float_to_e8m0_rn(x.y, sanitized_saturate);
        res = (__nv_fp8x2_storage_t)(res << 8U);
        res = (__nv_fp8x2_storage_t)(
            res | __internal_cvt_float_to_e8m0_rn(x.x, sanitized_saturate));
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundZero) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue8m0x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    }
#else
    res = __nv_cvt_float_to_e8m0(
        x.y, sanitized_saturate, sanitized_rounding);
    res <<= (unsigned short)8U;
    res |= __nv_cvt_float_to_e8m0(
        x.x, sanitized_saturate, sanitized_rounding);
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
float __internal_double_to_float_with_sticky(double x)
{
#if (defined __CUDA_ARCH__)
    // protect from ftz in device code
    float f;
    double d;
    asm("{  cvt.rn.f32.f64 %0, %1;}\n" : "=f"(f) : "d"(x));
    asm("{  cvt.f64.f32 %0, %1;}\n" : "=d"(d) : "f"(f));
#else
    const float f = (float)x;
    const double d = (double)f;
#endif
    unsigned int u = __internal_fp8_float_as_uint(f);
    int x_is_not_nan = ((u << (unsigned)1U) <= (unsigned)0xFF000000U) ? 1 : 0;

    if ((x > 0.0) && (d > x)) {
        u--;
    }
    if ((x < 0.0) && (d < x)) {
        u--;
    }
    if ((d != x) && (x_is_not_nan == 1)) {
        u |= 1U;
    }
    return __internal_fp8_uint_as_float(u);
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_double_to_e8m0(const double x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_e8m0_rounding(rounding);
    float fx_with_sticky = __internal_double_to_float_with_sticky(x);
    __nv_fp8_storage_t res = __nv_cvt_float_to_e8m0(
        fx_with_sticky, sanitized_saturate, sanitized_rounding);

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_double2_to_e8m0x2(const double2 x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_e8m0_rounding(rounding);
    float2 f;
    f.x = __internal_double_to_float_with_sticky(x.x);
    f.y = __internal_double_to_float_with_sticky(x.y);
    return __nv_cvt_float2_to_e8m0x2(
        f, sanitized_saturate, sanitized_rounding);
}

__CUDA_HOSTDEVICE_FP8_DECL__
unsigned short __internal_e8m0_to_bf16(const __nv_fp8_storage_t x)
{
    unsigned short res;
    // shift bias exponent bits into place
    res = ((unsigned short)x) << 7U;

    if (x == 0xFFU) {
        res = 0x7FFFU; // NaN --> Canonical QNaN
    } else if (x == 0U) {
        res = 0x0040U; // 2^-127
    }

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_bfloat16_raw __nv_cvt_e8m0_to_bf16raw(const __nv_fp8_storage_t x)
{
    __nv_bfloat16_raw res;

#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short in = (unsigned short)x;
    unsigned hr = 0U;
    asm("{cvt.rn.bf16x2.ue8m0x2 %0, %1;}\n"
                : "=r"(hr)
                : "h"(in));

    res.x = (unsigned short)hr;
#else
    res.x = __internal_e8m0_to_bf16(x);
#endif

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_bfloat162_raw __nv_cvt_e8m0x2_to_bf162raw(const __nv_fp8x2_storage_t x)
{
    __nv_bfloat162_raw res;

#if (__CUDA_FP8_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short in = (unsigned short)x;
    unsigned hr = 0U;
    asm("{cvt.rn.bf16x2.ue8m0x2 %0, %1;}\n"
                : "=r"(hr)
                : "h"(in));

    res.x = (unsigned short)hr;
    res.y = (unsigned short)(hr >> (unsigned)16U);
#else

    res.x = __internal_e8m0_to_bf16((__nv_fp8_storage_t)x);
    res.y = __internal_e8m0_to_bf16((__nv_fp8_storage_t)(x >> (unsigned short)8U));

#endif

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_halfraw_to_ue5m3(const __half_raw x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short ures = 0U;
    unsigned in = (unsigned)(x.x);
    if (((sanitized_rounding == cudaRoundZero) ||
         (sanitized_rounding == cudaRoundMinInf)) &&
        (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if (((sanitized_rounding == cudaRoundZero) ||
                (sanitized_rounding == cudaRoundMinInf)) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rn.satfinite.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rn.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    }
    return (__nv_fp8_storage_t)ures;
#else
    const float fx = __internal_halfraw_to_float(x);
    const __nv_fp8_storage_t res = __nv_cvt_float_to_ue5m3(
        fx, sanitized_saturate, sanitized_rounding);

    return res;
#endif
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_half2raw_to_ue5m3x2(const __half2_raw x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    __nv_fp8x2_storage_t ures = 0U;
    unsigned in = (unsigned)(x.x) | ((unsigned)(x.y) << (unsigned)16U);
    if (((sanitized_rounding == cudaRoundZero) ||
         (sanitized_rounding == cudaRoundMinInf)) &&
        (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if (((sanitized_rounding == cudaRoundZero) ||
                (sanitized_rounding == cudaRoundMinInf)) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rn.satfinite.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rn.ue5m3x2.f16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    }
    return ures;
#else
    __half_raw lo, hi;
    lo.x = x.x;
    hi.x = x.y;
    __nv_fp8x2_storage_t ures = __nv_cvt_halfraw_to_ue5m3(
        hi, sanitized_saturate, sanitized_rounding);
    ures <<= (unsigned short)8U;
    ures |= __nv_cvt_halfraw_to_ue5m3(
        lo, sanitized_saturate, sanitized_rounding);
    return ures;
#endif
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_bfloat16raw_to_ue5m3(const __nv_bfloat16_raw x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short ures = 0U;
    unsigned in = (unsigned)(x.x);
    if (((sanitized_rounding == cudaRoundZero) ||
         (sanitized_rounding == cudaRoundMinInf)) &&
        (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if (((sanitized_rounding == cudaRoundZero) ||
                (sanitized_rounding == cudaRoundMinInf)) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rn.satfinite.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rn.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    }
    return (__nv_fp8_storage_t)ures;
#else
    unsigned char res;
    unsigned short int xbits = x.x;

    const unsigned short int BF16_INF_BITS = 0x7F80U;
    const unsigned short int FP8_EXP_BIAS = 15U;
    const unsigned short int FP8_SIGNIFICAND_BITS = 4;
    const unsigned char      FP8_MANTISSA_MASK = 0x7U;
    const unsigned short int FP8_MINDENORM_O2 = 0x3680U; // 2^-18
    const unsigned short int FP8_OVERFLOW_THRESHOLD = 0x47E8U; // 2^17-2^14+2^12
    const unsigned char      FP8_MAXNORM = 0xFEU;
    const unsigned short int FP8_MINNORM = 0x3880; // 2^-14

    // 1/2 LSB of the target format, positioned in bf16 mantissa
    // helpful for midpoints detection during round-to-nearest-even step
    const unsigned short int FP8_BF16_HALF_ULP =
        (unsigned short)1U << (8U - FP8_SIGNIFICAND_BITS - 1U);
    // prepare exponent field in target format
    unsigned char exp =
        (unsigned char)((((unsigned short int)(xbits >> 7U)) & 0xFFU) -
                        127U + FP8_EXP_BIAS);
    // round mantissa to target format width, rounding towards zero
    unsigned char mantissa =
        (unsigned char)(xbits >> (8U - FP8_SIGNIFICAND_BITS)) &
        FP8_MANTISSA_MASK;
    unsigned short int absx = xbits & 0x7FFFU;

    if (absx <= FP8_MINDENORM_O2) {
        // zero or underflow
        res = 0U;
        if ((absx > 0U) && (sanitized_rounding == cudaRoundPosInf)) {
            res = 0x01U; // mindenorm
        }
    } else if (absx > BF16_INF_BITS) {
        // NaN
        res = 0xFFU;
    } else if (absx == BF16_INF_BITS) {
        // no inf in ue5m3, so return NaN unless saturate is __NV_SATFINITE
        if (sanitized_saturate == __NV_SATFINITE) {
            res = FP8_MAXNORM;
        } else {
            res = 0xFFU;
        }
    } else if (absx > FP8_OVERFLOW_THRESHOLD) {
        if ((sanitized_saturate == __NV_SATFINITE) ||
            (sanitized_rounding == cudaRoundZero) ||
            (sanitized_rounding == cudaRoundMinInf)) {
            res = FP8_MAXNORM;
        } else {
            // overflow to NaN
            res = 0xFFU;
        }
    } else if (absx >= FP8_MINNORM) {
        res = (unsigned char)((exp << (FP8_SIGNIFICAND_BITS - 1U)) | mantissa);
        // rounded-off bits
        unsigned short int round =
            xbits & ((FP8_BF16_HALF_ULP << 1U) - 1U);
        // adjustment for round-to-nearest-even and round-up
        if (
            ((sanitized_rounding == cudaRoundNearest) &&
                ((round > FP8_BF16_HALF_ULP) ||
                ((round == FP8_BF16_HALF_ULP) && (mantissa & 1U)))) ||
            ((sanitized_rounding == cudaRoundPosInf) && (round > 0U))
        ) {
            res = (unsigned char)(res + 1U);
        }
        if ((sanitized_saturate == __NV_SATFINITE) &&
            (res > FP8_MAXNORM)) {
            res = FP8_MAXNORM;
        }
    } else { // Denormal range
        unsigned char shift = (unsigned char)(1U - exp);
        // add implicit leading bit
        mantissa |= (unsigned char)(1U << (FP8_SIGNIFICAND_BITS - 1U));
        // additional round-off due to denormalization
        res = (unsigned char)(mantissa >> shift);

        // rounded-off bits, including implicit leading bit
        unsigned short int round =
            (xbits | ((unsigned short int)1U << (8U - 1U))) &
            ((FP8_BF16_HALF_ULP << (shift + 1U)) - 1U);
        // adjustment for round-to-nearest-even and round-up
        if (
            ((sanitized_rounding == cudaRoundNearest) &&
              ((round > (FP8_BF16_HALF_ULP << shift)) ||
              ((round == (FP8_BF16_HALF_ULP << shift)) && (res & 1U)))) ||
            ((sanitized_rounding == cudaRoundPosInf) && (round > 0U))
        ) {
            res = (unsigned char)(res + 1U);
        }
    }

    return (__nv_fp8_storage_t)res;
#endif
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_bfloat162raw_to_ue5m3x2(const __nv_bfloat162_raw x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    __nv_fp8x2_storage_t ures = 0U;
    unsigned in = (unsigned)(x.x) | ((unsigned)(x.y) << (unsigned)16U);
    if (((sanitized_rounding == cudaRoundZero) ||
         (sanitized_rounding == cudaRoundMinInf)) &&
        (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if (((sanitized_rounding == cudaRoundZero) ||
                (sanitized_rounding == cudaRoundMinInf)) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rn.satfinite.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rn.ue5m3x2.bf16x2 %0, %1;}\n"
                : "=h"(ures)
                : "r"(in));
    }
    return ures;
#else
    __nv_bfloat16_raw lo, hi;
    lo.x = x.x;
    hi.x = x.y;
    __nv_fp8x2_storage_t ures = __nv_cvt_bfloat16raw_to_ue5m3(
        hi, sanitized_saturate, sanitized_rounding);
    ures <<= (unsigned short)8U;
    ures |= __nv_cvt_bfloat16raw_to_ue5m3(
        lo, sanitized_saturate, sanitized_rounding);
    return ures;
#endif
}

__CUDA_HOSTDEVICE_FP8_DECL__ __nv_bfloat16_raw
__internal_float_to_bf16raw_rz_with_sticky(const float x) {
    // First get the rounded-to-zero BF16 value
    __nv_bfloat16_raw rz = __internal_float_to_bf16raw_rz(x);

    // Convert back to float to compare with original
    float rounded = __internal_bf16raw_to_float(rz);

    // Check if any bits were lost in the rounding
    // This sets the sticky bit if the rounded value is different from input
    if (rounded != x) {
        // Set the least significant bit of the mantissa as the sticky bit
        // NaN gets here too, but is not an issue
        rz.x |= 0x0001U;
    }

    return rz;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_float_to_ue5m3(const float x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
    __nv_fp8_storage_t res = 0U;
#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short ures = 0U;
    if (((sanitized_rounding == cudaRoundZero) ||
         (sanitized_rounding == cudaRoundMinInf)) &&
        (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if (((sanitized_rounding == cudaRoundZero) ||
                (sanitized_rounding == cudaRoundMinInf)) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rn.satfinite.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rn.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(ures)
                : "f"(x), "f"(0.0f));
    }
    res = (__nv_fp8_storage_t)ures;
#else
    if ((sanitized_rounding == cudaRoundZero) ||
        (sanitized_rounding == cudaRoundMinInf))
    {
        // convert to ue5m3 first discards the sign bit of the input, so rounding
        // towards negative is the same as rounding towards zero after that.
        res = __nv_cvt_bfloat16raw_to_ue5m3(
            __internal_float_to_bf16raw_rz(x), sanitized_saturate,
            sanitized_rounding);
    }
    else if (sanitized_rounding == cudaRoundPosInf)
    {
        float absx = __internal_fp8_uint_as_float((__internal_fp8_float_as_uint(x) << (unsigned)1U) >> (unsigned)1U);
        res = __nv_cvt_bfloat16raw_to_ue5m3(
            __internal_float_to_bf16raw_ru(absx), sanitized_saturate,
            sanitized_rounding);
    } else if (sanitized_rounding == cudaRoundNearest)
    {
        res = __nv_cvt_bfloat16raw_to_ue5m3(
            __internal_float_to_bf16raw_rz_with_sticky(x),
            sanitized_saturate, sanitized_rounding);
    }
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_float2_to_ue5m3x2(const float2 x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
    __nv_fp8x2_storage_t res = 0U;
#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    if (((sanitized_rounding == cudaRoundZero) ||
         (sanitized_rounding == cudaRoundMinInf)) &&
        (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rz.satfinite.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if (((sanitized_rounding == cudaRoundZero) ||
                (sanitized_rounding == cudaRoundMinInf)) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rz.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rp.satfinite.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundPosInf) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rp.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_SATFINITE)) {
        asm("{cvt.rn.satfinite.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    } else if ((sanitized_rounding == cudaRoundNearest) &&
               (sanitized_saturate == __NV_NOSAT)) {
        asm("{cvt.rn.ue5m3x2.f32 %0, %2, %1;}\n"
                : "=h"(res)
                : "f"(x.x), "f"(x.y));
    }
#else
    res = __nv_cvt_float_to_ue5m3(
        x.y, sanitized_saturate, sanitized_rounding);
    res <<= (unsigned short)8U;
    res |= __nv_cvt_float_to_ue5m3(
        x.x, sanitized_saturate, sanitized_rounding);
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8_storage_t __nv_cvt_double_to_ue5m3(const double x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
    float fx_with_sticky = __internal_double_to_float_with_sticky(x);
    __nv_fp8_storage_t res = __nv_cvt_float_to_ue5m3(
        fx_with_sticky, sanitized_saturate, sanitized_rounding);

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_fp8x2_storage_t __nv_cvt_double2_to_ue5m3x2(const double2 x,
    const __nv_saturation_t saturate,
    const enum cudaRoundMode rounding)
{
    const __nv_saturation_t sanitized_saturate =
        __internal_fp8_sanitize_saturation(saturate);
    const enum cudaRoundMode sanitized_rounding =
        __internal_fp8_sanitize_rounding(rounding);
    float2 f;
    f.x = __internal_double_to_float_with_sticky(x.x);
    f.y = __internal_double_to_float_with_sticky(x.y);
    return __nv_cvt_float2_to_ue5m3x2(
        f, sanitized_saturate, sanitized_rounding);
}

__CUDA_HOSTDEVICE_FP8_DECL__
unsigned short __internal_ue5m3_to_bf16(const __nv_fp8_storage_t x)
{
    unsigned short res;

    if (x == 0xFFU) {
        res = 0x7FFFU; // NaN --> NaN
    } else if (x == 0U) {
        res = 0U; // 0 --> +0
    } else if (x <= 0x07U) { // subnormal
        unsigned short msb;
        unsigned short mantissa = (unsigned short)x;
        unsigned short exponent = 127 - 14; // old Emin with the new bias
        do {
            msb = (mantissa & 0x4U);
            mantissa <<= 1U; // normalize
            --exponent;
        } while (msb == 0U);
        mantissa &= 0x7U; // wipe out the implicit bit
        res = (exponent << 7U) | (mantissa << 4U);
    } else { // normal
        // shift binary point into place
        res = ((unsigned short)x) << 4U;
        // add exponent bias
        res += ((unsigned short)(-15 + 127)) << 7U;
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_bfloat16_raw __nv_cvt_ue5m3_to_bf16raw(const __nv_fp8_storage_t x)
{
    __nv_bfloat16_raw res;

#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short in = (unsigned short)x;
    unsigned hr = 0U;
    asm("{cvt.rn.bf16x2.ue5m3x2 %0, %1;}\n"
                : "=r"(hr)
                : "h"(in));

    res.x = (unsigned short)hr;
#else
    res.x = __internal_ue5m3_to_bf16(x);
#endif

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__nv_bfloat162_raw __nv_cvt_ue5m3x2_to_bf162raw(const __nv_fp8x2_storage_t x)
{
    __nv_bfloat162_raw res;

#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short in = (unsigned short)x;
    unsigned hr = 0U;
    asm("{cvt.rn.bf16x2.ue5m3x2 %0, %1;}\n"
                : "=r"(hr)
                : "h"(in));

    res.x = (unsigned short)hr;
    res.y = (unsigned short)(hr >> (unsigned)16U);
#else

    res.x = __internal_ue5m3_to_bf16((__nv_fp8_storage_t)x);
    res.y = __internal_ue5m3_to_bf16((__nv_fp8_storage_t)(x >> (unsigned short)8U));

#endif

    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
unsigned short __internal_ue5m3_to_half(const __nv_fp8_storage_t x)
{
    unsigned short res;

    if (x == 0xFFU) {
        res = 0x7FFFU; // NaN --> NaN
    } else if (x <= 0xF7) {
        // zero, subnormal and most of the normal numbers
        res = (unsigned short)x << (unsigned short)7U;
    } else { // range overflow
        res = 0x7C00U; // +inf
    }
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__half_raw __nv_cvt_ue5m3_to_halfraw(const __nv_fp8_storage_t x)
{
    __half_raw res;

#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned short in = (unsigned short)x;
    unsigned hr = 0U;
    asm("{cvt.rn.f16x2.ue5m3x2 %0, %1;}\n": "=r"(hr): "h"(in));
    res.x = (unsigned short)hr;
#else
    res.x = __internal_ue5m3_to_half(x);
#endif
    return res;
}

__CUDA_HOSTDEVICE_FP8_DECL__
__half2_raw __nv_cvt_ue5m3x2_to_halfraw2(const __nv_fp8x2_storage_t x)
{
    __half2_raw res;

#if (__CUDA_FP8_UE5M3_INTERNAL_CAN_RELY_ON_PTX_FOR_SHORTTYPESCVT__)
    unsigned hr;
    asm("{cvt.rn.f16x2.ue5m3x2 %0, %1;}\n": "=r"(hr): "h"(x));
    res.x = (unsigned short)hr;
    res.y = (unsigned short)(hr >> (unsigned)16U);
#else
    res.x = __internal_ue5m3_to_half((__nv_fp8_storage_t)x);
    res.y = __internal_ue5m3_to_half((__nv_fp8_storage_t)(x >> (unsigned short)8U));
#endif
    return res;
}

#endif /* !(defined __DOXYGEN_ONLY__) */

/* All other definitions in this file are only visible to C++ compilers */
#if defined(__cplusplus)

#if defined(__CUDA_ARCH__) && (__CUDA_ARCH__ >= 900)
#define __NV_FP8_SM_90_TL__         __NV_TL__
#define __NV_FP8_SM_90_TL_BUILTIN__ __NV_TL_BUILTIN__
#else
#define __NV_FP8_SM_90_TL__
#define __NV_FP8_SM_90_TL_BUILTIN__
#endif

#if defined(__CUDA_ARCH__) && (__CUDA_ARCH__ >= 1000)
#define __NV_FP8_SM_100_TL__         __NV_TL__
#define __NV_FP8_SM_100_TL_BUILTIN__ __NV_TL_BUILTIN__
#else
#define __NV_FP8_SM_100_TL__
#define __NV_FP8_SM_100_TL_BUILTIN__
#endif

#if defined(__CUDA_ARCH__) && defined(__CUDA_ARCH_FAMILY_SPECIFIC__) && (__CUDA_ARCH_FAMILY_SPECIFIC__ == 1070)
#define __NV_FP8_SM_107F_TL__         __NV_TL__
#define __NV_FP8_SM_107F_TL_BUILTIN__ __NV_TL_BUILTIN__
#else
#define __NV_FP8_SM_107F_TL__
#define __NV_FP8_SM_107F_TL_BUILTIN__
#endif

/**
 * \defgroup CUDA_MATH_FP8_E5M2_STRUCT C++ struct for handling fp8 data type of e5m2 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8_E5M2_STRUCT
 * \brief __nv_fp8_e5m2 datatype
 *
 * \details This structure implements the datatype for handling
 * \p fp8 floating-point numbers of \p e5m2 kind:
 * with 1 sign, 5 exponent, 1 implicit and 2 explicit mantissa bits.
 *
 * The structure implements converting constructors and operators.
 */
struct __NV_TL_BUILTIN__ __CUDA_ALIGN__(1) __nv_fp8_e5m2 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8_E5M2_STRUCT
     * Storage variable contains the \p fp8 floating-point data.
     */
    __nv_fp8_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8_e5m2() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8_e5m2() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider FP types */
    /* Note we do avoid constructor init-list because of special host/device
     * compilation rules */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const __half f) {
        __x = __nv_cvt_halfraw_to_fp8(static_cast<__half_raw>(f),
                                      __NV_SATFINITE, __NV_E5M2);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat16 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const __nv_bfloat16 f) {
        __x = __nv_cvt_bfloat16raw_to_fp8(static_cast<__nv_bfloat16_raw>(f),
                                          __NV_SATFINITE, __NV_E5M2);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float data type, relies on \p __NV_SATFINITE behavior
     * for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const float f) {
        __x = __nv_cvt_float_to_fp8(f, __NV_SATFINITE, __NV_E5M2);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const double f) {
        __x = __nv_cvt_double_to_fp8(f, __NV_SATFINITE, __NV_E5M2);
    }

    /* Converts from integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p short \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__
    __nv_fp8_e5m2(const unsigned short int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const unsigned int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const unsigned long int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p long \p int data type, relies on
     * \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__
    __nv_fp8_e5m2(const unsigned long long int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p short \p int data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const short int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p int data type, relies on \p __NV_SATFINITE behavior
     * for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p int data type, relies on \p __NV_SATFINITE behavior
     * for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const long int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p long \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e5m2(const long long int val) {
        __x = static_cast<__nv_fp8_e5m2>(static_cast<float>(val)).__x;
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening FP converts */
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator __half() const {
        return static_cast<__half>(__nv_cvt_fp8_to_halfraw(__x, __NV_E5M2));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator float() const {
        return __internal_halfraw_to_float(
            __nv_cvt_fp8_to_halfraw(__x, __NV_E5M2));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat16 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator __nv_bfloat16() const {
        return static_cast<__nv_bfloat16>(__nv_cvt_fp8_to_bf16raw(__x, __NV_E5M2));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p double data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator double() const {
        return static_cast<double>(float(*this));
    }

    /* Convert to integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p char data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned char() const {
        unsigned char i;
        const float f = float(*this);
        const unsigned char max_val = 0xFFU;
        const unsigned char min_val = 0U;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if ((bits & 0x7FU) > 0x7CU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else if (f < static_cast<float>(min_val)) {
            // saturate minimum
            i = min_val;
        } else {
            // normal value
            i = static_cast<unsigned char>(f);
        }
        return i;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p short \p int data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned short int() const {
        return __half2ushort_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p int data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned int() const {
        return __half2uint_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p int data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ operator unsigned long int() const {
        unsigned long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(unsigned long) == sizeof(unsigned long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<unsigned long>(__half2ull_rz(__half(*this)));
        }
        else
        {
            retval = static_cast<unsigned long>(__half2uint_rz(__half(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p long \p int data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p 0x8000000000000000ULL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned long long int() const {
        return __half2ull_rz(__half(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p signed \p char data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator signed char() const {
        signed char i;
        const float f = float(*this);
        const signed char max_val = (signed char)0x7FU;
        const signed char min_val = (signed char)0x80U;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if ((bits & 0x7FU) > 0x7CU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else if (f < static_cast<float>(min_val)) {
            // saturate minimum
            i = min_val;
        } else {
            // normal value
            i = static_cast<signed char>(f);
        }
        return i;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to an implementation defined \p char data type.
     *
     * Detects signedness of the \p char type and proceeds accordingly, see
     * further details in signed and unsigned char operators.

     * Clamps inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ operator char() const {
        char value;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (((char)-1) < (char)0)
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            value = static_cast<char>(static_cast<signed char>(*this));
        }
        else
        {
            value = static_cast<char>(static_cast<unsigned char>(*this));
        }
        return value;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p short \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator short int() const {
        return __half2short_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator int() const {
        return __half2int_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ operator long int() const {
        long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(long) == sizeof(long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<long>(__half2ll_rz(__half(*this)));
        }
        else
        {
            retval = static_cast<long>(__half2int_rz(__half(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p 0x8000000000000000LL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator long long int() const {
        return __half2ll_rz(__half(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p bool data type.
     * +0 and -0 inputs convert to \p false.
     * Non-zero inputs convert to \p true.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator bool() const {
        return (__x & 0x7FU) != 0U;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X2_E5M2_STRUCT C++ struct for handling vector type of two fp8 values of e5m2 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X2_E5M2_STRUCT
 * \brief __nv_fp8x2_e5m2 datatype
 *
 * \details This structure implements the datatype for handling two
 * \p fp8 floating-point numbers of \p e5m2 kind each:
 * with 1 sign, 5 exponent, 1 implicit and 2 explicit mantissa bits.
 *
 * The structure implements converting constructors and operators.
 */
struct __CUDA_ALIGN__(2) __nv_fp8x2_e5m2 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X2_E5M2_STRUCT
     * Storage variable contains the vector of two \p fp8 floating-point data
     * values.
     */
    __nv_fp8x2_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x2_e5m2() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e5m2() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e5m2(const __half2 f) {
        __x = __nv_cvt_halfraw2_to_fp8x2(static_cast<__half2_raw>(f),
                                         __NV_SATFINITE, __NV_E5M2);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat162 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e5m2(const __nv_bfloat162 f) {
        __x = __nv_cvt_bfloat16raw2_to_fp8x2(static_cast<__nv_bfloat162_raw>(f),
                                             __NV_SATFINITE, __NV_E5M2);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e5m2(const float2 f) {
        __x = __nv_cvt_float2_to_fp8x2(f, __NV_SATFINITE, __NV_E5M2);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e5m2(const double2 f) {
        __x = __nv_cvt_double2_to_fp8x2(f, __NV_SATFINITE, __NV_E5M2);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __half2() const {
        return static_cast<__half2>(__nv_cvt_fp8x2_to_halfraw2(__x, __NV_E5M2));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat162 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __nv_bfloat162() const {
        return static_cast<__nv_bfloat162>(__nv_cvt_fp8x2_to_bf162raw(__x, __NV_E5M2));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float2() const {
        return __internal_halfraw2_to_float2(
            __nv_cvt_fp8x2_to_halfraw2(__x, __NV_E5M2));
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

__CUDA_HOSTDEVICE_FP8_DECL__ unsigned int
__internal_pack_u16x2_to_u32(const unsigned short int src_lo,
                             const unsigned short int src_hi) {
    unsigned int dst;
#if (defined __CUDACC__) && (defined __CUDA_ARCH__)
    asm("{  mov.b32 %0, {%1,%2};}\n" : "=r"(dst) : "h"(src_lo), "h"(src_hi));
#else
    dst = (static_cast<unsigned int>(src_hi) << 16U) |
          static_cast<unsigned int>(src_lo);
#endif
    return dst;
}

/**
 * \defgroup CUDA_MATH_FP8X4_E5M2_STRUCT C++ struct for handling vector type of four fp8 values of e5m2 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X4_E5M2_STRUCT
 * \brief __nv_fp8x4_e5m2 datatype
 *
 * \details This structure implements the datatype for handling four
 * \p fp8 floating-point numbers of \p e5m2 kind each:
 * with 1 sign, 5 exponent, 1 implicit and 2 explicit mantissa bits.
 *
 * The structure implements converting constructors and operators.
 */
struct __CUDA_ALIGN__(4) __nv_fp8x4_e5m2 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X4_E5M2_STRUCT
     * Storage variable contains the vector of four \p fp8 floating-point data
     * values.
     */
    __nv_fp8x4_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x4_e5m2() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __half2 data type values,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2(const __half2 flo,
                                                     const __half2 fhi) {
        const __nv_fp8x2_storage_t rlo = __nv_cvt_halfraw2_to_fp8x2(
            static_cast<__half2_raw>(flo), __NV_SATFINITE, __NV_E5M2);
        const __nv_fp8x2_storage_t rhi = __nv_cvt_halfraw2_to_fp8x2(
            static_cast<__half2_raw>(fhi), __NV_SATFINITE, __NV_E5M2);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __nv_bfloat162 data type values,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2(const __nv_bfloat162 flo,
                                                     const __nv_bfloat162 fhi) {
        const __nv_fp8x2_storage_t rlo = __nv_cvt_bfloat16raw2_to_fp8x2(
            static_cast<__nv_bfloat162_raw>(flo), __NV_SATFINITE, __NV_E5M2);
        const __nv_fp8x2_storage_t rhi = __nv_cvt_bfloat16raw2_to_fp8x2(
            static_cast<__nv_bfloat162_raw>(fhi), __NV_SATFINITE, __NV_E5M2);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float4 vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2(const float4 f) {
        const float2 flo = {f.x, f.y};
        const float2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_float2_to_fp8x2(flo, __NV_SATFINITE, __NV_E5M2);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_float2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E5M2);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4 vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    __NV_SILENCE_DEPRECATION_BEGIN
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2(const double4 f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_fp8x2(flo, __NV_SATFINITE, __NV_E5M2);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E5M2);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    __NV_SILENCE_DEPRECATION_END
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_16a vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2(const double4_16a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_fp8x2(flo, __NV_SATFINITE, __NV_E5M2);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E5M2);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_32a vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e5m2(const double4_32a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_fp8x2(flo, __NV_SATFINITE, __NV_E5M2);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E5M2);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float4 vector data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float4() const {
        const __nv_fp8x2_storage_t slo = static_cast<__nv_fp8x2_storage_t>(__x);
        const __nv_fp8x2_storage_t shi =
            static_cast<__nv_fp8x2_storage_t>(__x >> 16U);
        float2 rlo = __internal_halfraw2_to_float2(
            __nv_cvt_fp8x2_to_halfraw2(slo, __NV_E5M2));
        float2 rhi = __internal_halfraw2_to_float2(
            __nv_cvt_fp8x2_to_halfraw2(shi, __NV_E5M2));
        float4 res = {rlo.x, rlo.y, rhi.x, rhi.y};
        return res;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8_E4M3_STRUCT C++ struct for handling fp8 data type of e4m3 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8_E4M3_STRUCT
 * \brief __nv_fp8_e4m3 datatype
 *
 * \details This structure implements the datatype for storing
 * \p fp8 floating-point numbers of \p e4m3 kind:
 * with 1 sign, 4 exponent, 1 implicit and 3 explicit mantissa bits.
 * The encoding doesn't support Infinity.
 * NaNs are limited to 0x7F and 0xFF values.
 *
 * The structure implements converting constructors and operators.
 */
struct __NV_TL_BUILTIN__ __CUDA_ALIGN__(1) __nv_fp8_e4m3 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8_E4M3_STRUCT
     * Storage variable contains the \p fp8 floating-point data.
     */
    __nv_fp8_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8_e4m3() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8_e4m3() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider FP types */
    /* Note we do avoid constructor init-list because of special host/device
     * compilation rules */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const __half f) {
        __x = __nv_cvt_halfraw_to_fp8(static_cast<__half_raw>(f),
                                      __NV_SATFINITE, __NV_E4M3);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat16 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const __nv_bfloat16 f) {
        __x = __nv_cvt_bfloat16raw_to_fp8(static_cast<__nv_bfloat16_raw>(f),
                                          __NV_SATFINITE, __NV_E4M3);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float data type, relies on \p __NV_SATFINITE behavior
     * for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const float f) {
        __x = __nv_cvt_float_to_fp8(f, __NV_SATFINITE, __NV_E4M3);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const double f) {
        __x = __nv_cvt_double_to_fp8(f, __NV_SATFINITE, __NV_E4M3);
    }

    /* Converts from integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p short \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__
    __nv_fp8_e4m3(const unsigned short int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const unsigned int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const unsigned long int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p long \p int data type, relies on
     * \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__
    __nv_fp8_e4m3(const unsigned long long int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p short \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const short int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p int data type, relies on \p __NV_SATFINITE behavior
     * for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const long int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p long \p int data type, relies on \p
     * __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ __nv_fp8_e4m3(const long long int val) {
        __x = static_cast<__nv_fp8_e4m3>(static_cast<float>(val)).__x;
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening FP converts */
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator __half() const {
        return static_cast<__half>(__nv_cvt_fp8_to_halfraw(__x, __NV_E4M3));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator float() const {
        return __internal_halfraw_to_float(
            __nv_cvt_fp8_to_halfraw(__x, __NV_E4M3));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat16 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator __nv_bfloat16() const {
        return static_cast<__nv_bfloat16>(__nv_cvt_fp8_to_bf16raw(__x, __NV_E4M3));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p double data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator double() const {
        return static_cast<double>(float(*this));
    }

    /* Convert to integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p char data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned char() const {
        unsigned char i;
        const float f = float(*this);
        const unsigned char max_val = 0xFFU;
        const unsigned char min_val = 0U;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if ((bits & 0x7FU) == 0x7FU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else if (f < static_cast<float>(min_val)) {
            // saturate minimum
            i = min_val;
        } else {
            // normal value
            i = static_cast<unsigned char>(f);
        }
        return i;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p short \p int data type.
     * Clamps negative inputs to zero.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned short int() const {
        return __half2ushort_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p int data type.
     * Clamps negative inputs to zero.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned int() const {
        return __half2uint_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p int data type.
     * Clamps negative and too large inputs to the output range.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ operator unsigned long int() const {
        unsigned long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(unsigned long) == sizeof(unsigned long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<unsigned long>(__half2ull_rz(__half(*this)));
        }
        else
        {
            retval = static_cast<unsigned long>(__half2uint_rz(__half(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p long \p int data type.
     * Clamps negative inputs to zero.
     * NaN inputs convert to \p 0x8000000000000000ULL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator unsigned long long int() const {
        return __half2ull_rz(__half(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p signed \p char data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator signed char() const {
        signed char i;
        const float f = float(*this);
        const signed char max_val = (signed char)0x7FU;
        const signed char min_val = (signed char)0x80U;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if ((bits & 0x7FU) == 0x7FU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else if (f < static_cast<float>(min_val)) {
            // saturate minimum
            i = min_val;
        } else {
            // normal value
            i = static_cast<signed char>(f);
        }
        return i;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to an implementation defined \p char data type.
     *
     * Detects signedness of the \p char type and proceeds accordingly, see
     * further details in signed and unsigned char operators.

     * Clamps inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ operator char() const {
        char value;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (((char)-1) < (char)0)
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            value = static_cast<char>(static_cast<signed char>(*this));
        }
        else
        {
            value = static_cast<char>(static_cast<unsigned char>(*this));
        }
        return value;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p short \p int data type.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator short int() const {
        return __half2short_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p int data type.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator int() const {
        return __half2int_rz(__half(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ operator long int() const {
        long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(long) == sizeof(long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<long>(__half2ll_rz(__half(*this)));
        }
        else
        {
            retval = static_cast<long>(__half2int_rz(__half(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p long \p int data type.
     * NaN inputs convert to \p 0x8000000000000000LL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator long long int() const {
        return __half2ll_rz(__half(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p bool data type.
     * +0 and -0 inputs convert to \p false.
     * Non-zero inputs convert to \p true.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_90_TL__ __NV_FP8_SM_90_TL_BUILTIN__ operator bool() const {
        return (__x & 0x7FU) != 0U;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X2_E4M3_STRUCT C++ struct for handling vector type of two fp8 values of e4m3 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X2_E4M3_STRUCT
 * \brief __nv_fp8x2_e4m3 datatype
 *
 * \details This structure implements the datatype for storage
 * and operations on the vector of two \p fp8 values of \p e4m3 kind each:
 * with 1 sign, 4 exponent, 1 implicit and 3 explicit mantissa bits.
 * The encoding doesn't support Infinity.
 * NaNs are limited to 0x7F and 0xFF values.
 */
struct __CUDA_ALIGN__(2) __nv_fp8x2_e4m3 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X2_E4M3_STRUCT
     * Storage variable contains the vector of two \p fp8 floating-point data
     * values.
     */
    __nv_fp8x2_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x2_e4m3() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e4m3() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e4m3(const __half2 f) {
        __x = __nv_cvt_halfraw2_to_fp8x2(static_cast<__half2_raw>(f),
                                         __NV_SATFINITE, __NV_E4M3);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat162 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e4m3(const __nv_bfloat162 f) {
        __x = __nv_cvt_bfloat16raw2_to_fp8x2(static_cast<__nv_bfloat162_raw>(f),
                                             __NV_SATFINITE, __NV_E4M3);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e4m3(const float2 f) {
        __x = __nv_cvt_float2_to_fp8x2(f, __NV_SATFINITE, __NV_E4M3);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e4m3(const double2 f) {
        __x = __nv_cvt_double2_to_fp8x2(f, __NV_SATFINITE, __NV_E4M3);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __half2() const {
        return static_cast<__half2>(__nv_cvt_fp8x2_to_halfraw2(__x, __NV_E4M3));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat162 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __nv_bfloat162() const {
        return static_cast<__nv_bfloat162>(__nv_cvt_fp8x2_to_bf162raw(__x, __NV_E4M3));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float2() const {
        return __internal_halfraw2_to_float2(
            __nv_cvt_fp8x2_to_halfraw2(__x, __NV_E4M3));
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X4_E4M3_STRUCT C++ struct for handling vector type of four fp8 values of e4m3 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X4_E4M3_STRUCT
 * \brief __nv_fp8x4_e4m3 datatype
 *
 * \details This structure implements the datatype for storage
 * and operations on the vector of four \p fp8 values of \p e4m3 kind each:
 * with 1 sign, 4 exponent, 1 implicit and 3 explicit mantissa bits.
 * The encoding doesn't support Infinity.
 * NaNs are limited to 0x7F and 0xFF values.
 */
struct __CUDA_ALIGN__(4) __nv_fp8x4_e4m3 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X4_E4M3_STRUCT
     * Storage variable contains the vector of four \p fp8 floating-point data
     * values.
     */
    __nv_fp8x4_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x4_e4m3() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __half2 data type values,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3(const __half2 flo,
                                                     const __half2 fhi) {
        const __nv_fp8x2_storage_t rlo = __nv_cvt_halfraw2_to_fp8x2(
            static_cast<__half2_raw>(flo), __NV_SATFINITE, __NV_E4M3);
        const __nv_fp8x2_storage_t rhi = __nv_cvt_halfraw2_to_fp8x2(
            static_cast<__half2_raw>(fhi), __NV_SATFINITE, __NV_E4M3);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __nv_bfloat162 data type values,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3(const __nv_bfloat162 flo,
                                                     const __nv_bfloat162 fhi) {
        const __nv_fp8x2_storage_t rlo = __nv_cvt_bfloat16raw2_to_fp8x2(
            static_cast<__nv_bfloat162_raw>(flo), __NV_SATFINITE, __NV_E4M3);
        const __nv_fp8x2_storage_t rhi = __nv_cvt_bfloat16raw2_to_fp8x2(
            static_cast<__nv_bfloat162_raw>(fhi), __NV_SATFINITE, __NV_E4M3);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float4 vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3(const float4 f) {
        const float2 flo = {f.x, f.y};
        const float2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_float2_to_fp8x2(flo, __NV_SATFINITE, __NV_E4M3);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_float2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E4M3);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4 vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    __NV_SILENCE_DEPRECATION_BEGIN
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3(const double4 f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_fp8x2(flo, __NV_SATFINITE, __NV_E4M3);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E4M3);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    __NV_SILENCE_DEPRECATION_END
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_16a vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3(const double4_16a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_fp8x2(flo, __NV_SATFINITE, __NV_E4M3);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E4M3);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_32a vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e4m3(const double4_32a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_fp8x2(flo, __NV_SATFINITE, __NV_E4M3);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_fp8x2(fhi, __NV_SATFINITE, __NV_E4M3);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float4 vector data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float4() const {
        const __nv_fp8x2_storage_t slo = static_cast<__nv_fp8x2_storage_t>(__x);
        const __nv_fp8x2_storage_t shi =
            static_cast<__nv_fp8x2_storage_t>(__x >> 16U);
        float2 rlo = __internal_halfraw2_to_float2(
            __nv_cvt_fp8x2_to_halfraw2(slo, __NV_E4M3));
        float2 rhi = __internal_halfraw2_to_float2(
            __nv_cvt_fp8x2_to_halfraw2(shi, __NV_E4M3));
        float4 res = {rlo.x, rlo.y, rhi.x, rhi.y};
        return res;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8_E8M0_STRUCT C++ struct for handling scale factor data type of e8m0 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8_E8M0_STRUCT
 * \brief __nv_fp8_e8m0 datatype
 *
 * \details This structure implements the datatype for handling
 * 8-bit scale factors of \p e8m0 kind: interpreted as powers of two
 * with biased exponent. Bias equals 127, so numbers 0 through 254
 * represent 2^-127 through 2^127. Number \p 0xFF = 255 is reserved
 * for NaN.
 *
 * The structure implements converting constructors and operators.
 */
struct __NV_TL_BUILTIN__ __CUDA_ALIGN__(1) __nv_fp8_e8m0 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8_E8M0_STRUCT
     * Storage variable contains the 8-bit scale data.
     */
    __nv_fp8_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8_e8m0() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8_e8m0() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider FP types */
    /* Note we do avoid constructor init-list because of special host/device
     * compilation rules */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundPosInf for
     * rounding.
     * \see __nv_cvt_float_to_e8m0 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const __half f) {
        __x = __nv_cvt_float_to_e8m0(__internal_halfraw_to_float(static_cast<__half_raw>(f)),
                                     __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat16 data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundPosInf for
     * rounding.
     * \see __nv_cvt_bfloat16raw_to_e8m0 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const __nv_bfloat16 f) {
        __x = __nv_cvt_bfloat16raw_to_e8m0(static_cast<__nv_bfloat16_raw>(f),
                                           __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float data type, relies on \p __NV_SATFINITE behavior
     * behavior for large input values and \p cudaRoundPosInf for
     * rounding.
     * \see __nv_cvt_float_to_e8m0 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const float f) {
        __x = __nv_cvt_float_to_e8m0(f, __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundPosInf for
     * rounding.
     * \see __nv_cvt_double_to_e8m0 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const double f) {
        __x = __nv_cvt_double_to_e8m0(f, __NV_SATFINITE, cudaRoundPosInf);
    }

    /* Converts from integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p short \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__
    __nv_fp8_e8m0(const unsigned short int val) {
        __x = static_cast<__nv_fp8_e8m0>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const unsigned int val) {
        __x = static_cast<__nv_fp8_e8m0>(static_cast<double>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p long \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__
    __nv_fp8_e8m0(const unsigned long long int val) {
        __nv_bfloat16 rn = __ull2bfloat16_rn(val);
        __nv_bfloat16_raw rn_raw = static_cast<__nv_bfloat16_raw>(rn);
        unsigned long long int back_int = __bfloat162ull_rz(rn);
        if (back_int < val)
        {
            rn_raw.x++;
        }
        __x = __nv_cvt_bfloat16raw_to_e8m0(rn_raw, __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const unsigned long int val) {
        __x = static_cast<__nv_fp8_e8m0>(static_cast<unsigned long long int>(val)).__x;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p short \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const short int val) {
        __x = static_cast<__nv_fp8_e8m0>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p int data type, relies on
    * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const int val) {
        __x = static_cast<__nv_fp8_e8m0>(static_cast<double>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p long \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const long long int val) {
        __nv_bfloat16 rn = __ll2bfloat16_rn(val);
        __nv_bfloat16_raw rn_raw = static_cast<__nv_bfloat16_raw>(rn);
        long long int back_int = __bfloat162ll_rz(rn);
        if (((val > 0) && (back_int < val)) || ((val < 0) && (back_int > val)))
        {
            rn_raw.x++;
        }
        __x = __nv_cvt_bfloat16raw_to_e8m0(rn_raw, __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p int data type, relies on
     * \p cudaRoundPosInf rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ __nv_fp8_e8m0(const long int val) {
        __x = static_cast<__nv_fp8_e8m0>(static_cast<long long int>(val)).__x;
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening FP converts */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator float() const {
        return __internal_bf16raw_to_float(__nv_cvt_e8m0_to_bf16raw((*this).__x));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat16 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator __nv_bfloat16() const {
        return static_cast<__nv_bfloat16>(
            __nv_cvt_e8m0_to_bf16raw((*this).__x));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p double data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator double() const {
        const float f = float(*this);
        double d;
#if (defined __CUDA_ARCH__)
        // protect from ftz in device code
        asm("{  cvt.f64.f32 %0, %1;}\n" : "=d"(d) : "f"(f));
#else
        d = static_cast<double>(f);
#endif
        return d;
    }

    /* rounding conversion to half */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator __half() const {
        return __float2half_rn(float(*this));
    }

    /* Convert to integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p char data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator unsigned char() const {
        unsigned char i;
        const float f = float(*this);
        const unsigned char max_val = 0xFFU;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if (bits == 0xFFU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else {
            // normal value
            i = static_cast<unsigned char>(f);
        }
        return i;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p short \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator unsigned short int() const {
        return __bfloat162ushort_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator unsigned int() const {
        return __bfloat162uint_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator unsigned long int() const {
        unsigned long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(unsigned long) == sizeof(unsigned long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<unsigned long>(__bfloat162ull_rz(__nv_bfloat16(*this)));
        }
        else
        {
            retval = static_cast<unsigned long>(__bfloat162uint_rz(__nv_bfloat16(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p 0x8000000000000000ULL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator unsigned long long int() const {
        return __bfloat162ull_rz(__nv_bfloat16(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p signed \p char data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator signed char() const {
        signed char i;
        const float f = float(*this);
        const signed char max_val = (signed char)0x7FU;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if (bits == 0xFFU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else {
            // normal value
            i = static_cast<signed char>(f);
        }
        return i;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to an implementation defined \p char data type.
     *
     * Detects signedness of the \p char type and proceeds accordingly, see
     * further details in signed and unsigned char operators.

     * Clamps inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator char() const {
        char value;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (((char)-1) < (char)0)
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            value = static_cast<char>(static_cast<signed char>(*this));
        }
        else
        {
            value = static_cast<char>(static_cast<unsigned char>(*this));
        }
        return value;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p short \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator short int() const {
        return __bfloat162short_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator int() const {
        return __bfloat162int_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator long int() const {
        long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(long) == sizeof(long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<long>(__bfloat162ll_rz(__nv_bfloat16(*this)));
        }
        else
        {
            retval = static_cast<long>(__bfloat162int_rz(__nv_bfloat16(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p long \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p 0x8000000000000000LL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator long long int() const {
        return __bfloat162ll_rz(__nv_bfloat16(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p bool data type.
     * All values in input range are non-zero, so result is always \p true.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_100_TL__ __NV_FP8_SM_100_TL_BUILTIN__ operator bool() const {
        return true;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X2_E8M0_STRUCT C++ struct for handling vector type of two scale factors of e8m0 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X2_E8M0_STRUCT
 * \brief __nv_fp8x2_e8m0 datatype
 *
 * \details This structure implements the datatype for storage
 * and operations on the vector of two scale factors of \p e8m0 kind each.
 */
struct __CUDA_ALIGN__(2) __nv_fp8x2_e8m0 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X2_E8M0_STRUCT
     * Storage variable contains the vector of two scale factor
     * values.
     */
    __nv_fp8x2_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x2_e8m0() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e8m0() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e8m0(const __half2 f) {
        __x = __nv_cvt_float2_to_e8m0x2(__half22float2(f),
                                __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat162 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e8m0(const __nv_bfloat162 f) {
        __x = __nv_cvt_bfloat162raw_to_e8m0x2(static_cast<__nv_bfloat162_raw>(f),
                                             __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e8m0(const float2 f) {
        __x = __nv_cvt_float2_to_e8m0x2(f,
                                __NV_SATFINITE, cudaRoundPosInf);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double2 data type, relies on \p __NV_SATFINITE
     * behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_e8m0(const double2 f) {
        __x = __nv_cvt_double2_to_e8m0x2(f, __NV_SATFINITE, cudaRoundPosInf);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat162 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __nv_bfloat162() const {
        return static_cast<__nv_bfloat162>(__nv_cvt_e8m0x2_to_bf162raw((*this).__x));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float2() const {
        return __bfloat1622float2(static_cast<__nv_bfloat162>(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __half2() const {
        return __float22half2_rn(static_cast<float2>(*this));
    }

#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X4_E8M0_STRUCT C++ struct for handling vector type of four scale factors of e8m0 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X4_E8M0_STRUCT
 * \brief __nv_fp8x4_e8m0 datatype
 *
 * \details This structure implements the datatype for storage
 * and operations on the vector of scale factors of \p e8m0 kind each.
 */
struct __CUDA_ALIGN__(4) __nv_fp8x4_e8m0 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X4_E8M0_STRUCT
     * Storage variable contains the vector of four scale factor
     * values.
     */
    __nv_fp8x4_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x4_e8m0() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __half2 data type values,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0(const __half2 flo,
                                                     const __half2 fhi) {
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_e8m0>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_e8m0>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __nv_bfloat162 data type values,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0(const __nv_bfloat162 flo,
                                                     const __nv_bfloat162 fhi) {
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_e8m0>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_e8m0>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float4 vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0(const float4 f) {
        const float2 flo = {f.x, f.y};
        const float2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_float2_to_e8m0x2(flo, __NV_SATFINITE, cudaRoundPosInf);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_float2_to_e8m0x2(fhi, __NV_SATFINITE, cudaRoundPosInf);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4 vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    __NV_SILENCE_DEPRECATION_BEGIN
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0(const double4 f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_e8m0x2(flo, __NV_SATFINITE, cudaRoundPosInf);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_e8m0x2(fhi, __NV_SATFINITE, cudaRoundPosInf);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    __NV_SILENCE_DEPRECATION_END

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_16a vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0(const double4_16a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_e8m0x2(flo, __NV_SATFINITE, cudaRoundPosInf);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_e8m0x2(fhi, __NV_SATFINITE, cudaRoundPosInf);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_32a vector data type,
     * relies on \p __NV_SATFINITE behavior for out-of-range values.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_e8m0(const double4_32a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo =
            __nv_cvt_double2_to_e8m0x2(flo, __NV_SATFINITE, cudaRoundPosInf);
        const __nv_fp8x2_storage_t rhi =
            __nv_cvt_double2_to_e8m0x2(fhi, __NV_SATFINITE, cudaRoundPosInf);
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float4 vector data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float4() const {
        __nv_fp8x2_e8m0 lo;
        lo.__x = static_cast<__nv_fp8x2_storage_t>(__x);
        __nv_fp8x2_e8m0 hi;
        hi.__x = static_cast<__nv_fp8x2_storage_t>(__x >> 16U);

        float2 rlo = static_cast<float2>(lo);
        float2 rhi = static_cast<float2>(hi);
        float4 res = {rlo.x, rlo.y, rhi.x, rhi.y};
        return res;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8_UE5M3_STRUCT C++ struct for handling fp8 data type of ue5m3 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8_UE5M3_STRUCT
 * \brief __nv_fp8_ue5m3 datatype
 *
 * \details This structure implements the datatype for handling
 * \p fp8 floating-point numbers of \p ue5m3 kind:
 * with no sign, 5 exponent, 1 implicit and 3 explicit mantissa bits.
 * The encoding doesn't support Infinity.
 * NaNs are limited to 0xFF values.
 *
 * The structure implements converting constructors and operators.
 */
struct __NV_TL_BUILTIN__ __CUDA_ALIGN__(1) __nv_fp8_ue5m3 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8_UE5M3_STRUCT
     * Storage variable contains the \p fp8 floating-point data.
     */
    __nv_fp8_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8_ue5m3() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8_ue5m3() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider FP types */
    /* Note we do avoid constructor init-list because of special host/device
     * compilation rules */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_halfraw_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const __half f) {
        __x = __nv_cvt_halfraw_to_ue5m3(static_cast<__half_raw>(f),
                                     __NV_SATFINITE, cudaRoundNearest);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat16 data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_bfloat16raw_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const __nv_bfloat16 f) {
        __x = __nv_cvt_bfloat16raw_to_ue5m3(static_cast<__nv_bfloat16_raw>(f),
                                           __NV_SATFINITE, cudaRoundNearest);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_float_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const float f) {
        __x = __nv_cvt_float_to_ue5m3(f, __NV_SATFINITE, cudaRoundNearest);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_double_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const double f) {
        __x = __nv_cvt_double_to_ue5m3(f, __NV_SATFINITE, cudaRoundNearest);
    }

    /* Converts from integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p short \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__
    __nv_fp8_ue5m3(const unsigned short int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const unsigned int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p long \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__
    __nv_fp8_ue5m3(const unsigned long long int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p unsigned \p long \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const unsigned long int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p short \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const short int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p int data type, relies on
    * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p long \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const long long int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p long \p int data type, relies on
     * \p cudaRoundNearest rounding.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ __nv_fp8_ue5m3(const long int val) {
        __x = static_cast<__nv_fp8_ue5m3>(static_cast<float>(val)).__x;
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening FP converts */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator float() const {
        return __bfloat162float(static_cast<__nv_bfloat16>(__nv_cvt_ue5m3_to_bf16raw((*this).__x)));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat16 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator __nv_bfloat16() const {
        return static_cast<__nv_bfloat16>(
            __nv_cvt_ue5m3_to_bf16raw((*this).__x));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p double data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator double() const {
        const float f = float(*this);
        double d;
#if (defined __CUDA_ARCH__)
        // protect from ftz in device code
        asm("{  cvt.f64.f32 %0, %1;}\n" : "=d"(d) : "f"(f));
#else
        d = static_cast<double>(f);
#endif
        return d;
    }

    /* rounding conversion to half */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator __half() const {
        return static_cast<__half>(
            __nv_cvt_ue5m3_to_halfraw((*this).__x));
    }

    /* Convert to integral */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p char data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator unsigned char() const {
        unsigned char i;
        const float f = float(*this);
        const unsigned char max_val = 0xFFU;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if (bits == 0xFFU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else {
            // normal value
            i = static_cast<unsigned char>(f);
        }
        return i;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p short \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator unsigned short int() const {
        return __bfloat162ushort_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p int data type.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator unsigned int() const {
        return __bfloat162uint_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p int data type.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator unsigned long int() const {
        unsigned long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(unsigned long) == sizeof(unsigned long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<unsigned long>(__bfloat162ull_rz(__nv_bfloat16(*this)));
        }
        else
        {
            retval = static_cast<unsigned long>(__bfloat162uint_rz(__nv_bfloat16(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p unsigned \p long \p long \p int data type.
     * NaN inputs convert to \p 0x8000000000000000ULL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator unsigned long long int() const {
        return __bfloat162ull_rz(__nv_bfloat16(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p signed \p char data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator signed char() const {
        signed char i;
        const float f = float(*this);
        const signed char max_val = (signed char)0x7FU;
        const unsigned char bits = (*this).__x;
        // saturation fixup
        if (bits == 0xFFU) {
            // NaN
            i = 0;
        } else if (f > static_cast<float>(max_val)) {
            // saturate maximum
            i = max_val;
        } else {
            // normal value
            i = static_cast<signed char>(f);
        }
        return i;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to an implementation defined \p char data type.
     *
     * Detects signedness of the \p char type and proceeds accordingly, see
     * further details in signed and unsigned char operators.

     * Clamps inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator char() const {
        char value;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (((char)-1) < (char)0)
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            value = static_cast<char>(static_cast<signed char>(*this));
        }
        else
        {
            value = static_cast<char>(static_cast<unsigned char>(*this));
        }
        return value;
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p short \p int data type.
     * Clamps too large inputs to the output range.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator short int() const {
        return __bfloat162short_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p int data type.
     * NaN inputs convert to \p zero.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator int() const {
        return __bfloat162int_rz(__nv_bfloat16(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p int data type.
     * NaN inputs convert to \p zero if output type is 32-bit.
     * NaN inputs convert to \p 0x8000000000000000ULL if output type is 64-bit.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator long int() const {
        long retval;
        /* Suppress VS warning: warning C4127: conditional expression is constant */
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (push)
#pragma warning (disable: 4127)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        if (sizeof(long) == sizeof(long long))
#if defined(_MSC_VER) && !defined(__CUDA_ARCH__)
#pragma warning (pop)
#endif /* _MSC_VER && !defined(__CUDA_ARCH__) */
        {
            retval = static_cast<long>(__bfloat162ll_rz(__nv_bfloat16(*this)));
        }
        else
        {
            retval = static_cast<long>(__bfloat162int_rz(__nv_bfloat16(*this)));
        }
        return retval;
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p long \p long \p int data type.
     * NaN inputs convert to \p 0x8000000000000000LL.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator long long int() const {
        return __bfloat162ll_rz(__nv_bfloat16(*this));
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p bool data type.
     * Zero converts \p false, all other values, including NaN, convert \p true.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __NV_FP8_SM_107F_TL__ __NV_FP8_SM_107F_TL_BUILTIN__ operator bool() const {
        return (__x != 0x0U);
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X2_UE5M3_STRUCT C++ struct for handling vector type of two fp8 values of ue5m3 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X2_UE5M3_STRUCT
 * \brief __nv_fp8x2_ue5m3 datatype
 *
 * \details This structure implements the datatype for storage
 * and operations on the vector of two \p fp8 values of \p ue5m3 kind each:
 * with no sign, 5 exponent, 1 implicit and 3 explicit mantissa bits.
 * The encoding doesn't support Infinity.
 * NaNs are limited to 0xFF values.
 */
struct __CUDA_ALIGN__(2) __nv_fp8x2_ue5m3 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X2_UE5M3_STRUCT
     * Storage variable contains the vector of two \p fp8 floating-point data
     * values.
     */
    __nv_fp8x2_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x2_ue5m3() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_ue5m3() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __half2 data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_halfraw_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_ue5m3(const __half2 f) {
        __x = __nv_cvt_half2raw_to_ue5m3x2(static_cast<__half2_raw>(f), __NV_SATFINITE, cudaRoundNearest);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p __nv_bfloat162 data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_bfloat16raw_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_ue5m3(const __nv_bfloat162 f) {
        __x = __nv_cvt_bfloat162raw_to_ue5m3x2(static_cast<__nv_bfloat162_raw>(f),
                                             __NV_SATFINITE, cudaRoundNearest);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float2 data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_float_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_ue5m3(const float2 f) {
        __x = __nv_cvt_float2_to_ue5m3x2(f, __NV_SATFINITE, cudaRoundNearest);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double2 data type, relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_double_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x2_ue5m3(const double2 f) {
        __x = __nv_cvt_double2_to_ue5m3x2(f, __NV_SATFINITE, cudaRoundNearest);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __nv_bfloat162 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __nv_bfloat162() const {
        return static_cast<__nv_bfloat162>(__nv_cvt_ue5m3x2_to_bf162raw((*this).__x));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float2() const {
        return __bfloat1622float2(static_cast<__nv_bfloat162>(*this));
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p __half2 data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator __half2() const {
        return static_cast<__half2>(
            __nv_cvt_ue5m3x2_to_halfraw2((*this).__x));
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

/**
 * \defgroup CUDA_MATH_FP8X4_UE5M3_STRUCT C++ struct for handling vector type of four fp8 values of ue5m3 kind.
 * \ingroup CUDA_MATH_INTRINSIC_FP8
 */

/**
 * \ingroup CUDA_MATH_FP8X4_UE5M3_STRUCT
 * \brief __nv_fp8x4_ue5m3 datatype
 *
 * \details This structure implements the datatype for storage
 * and operations on the vector of four \p fp8 values of \p ue5m3 kind each:
 * with no sign, 5 exponent, 1 implicit and 3 explicit mantissa bits.
 * The encoding doesn't support Infinity.
 * NaNs are limited to 0xFF values.
 */
struct __CUDA_ALIGN__(4) __nv_fp8x4_ue5m3 {
  public:
    /**
     * \ingroup CUDA_MATH_FP8X4_UE5M3_STRUCT
     * Storage variable contains the vector of four \p fp8 floating-point data
     * values.
     */
    __nv_fp8x4_storage_t __x;

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor by default.
     */
#if defined(__CPP_VERSION_AT_LEAST_11_FP8)
    __nv_fp8x4_ue5m3() = default;
#else
    __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3() {}
#endif /* defined(__CPP_VERSION_AT_LEAST_11_FP8) */

#if !defined(__CUDA_NO_FP8_CONVERSIONS__)

    /* Construct from wider types */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __half2 data type values,
     * relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_float_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3(const __half2 flo,
                                                     const __half2 fhi) {
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_ue5m3>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_ue5m3>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from a pair of \p __nv_bfloat162 data type values,
     * relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_bfloat16raw_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3(const __nv_bfloat162 flo,
                                                     const __nv_bfloat162 fhi) {
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_ue5m3>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_ue5m3>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p float4 vector data type,
     * relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_float_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3(const float4 f) {
        const float2 flo = {f.x, f.y};
        const float2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_ue5m3>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_ue5m3>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4 vector data type,
     * relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_double_to_ue5m3 for further details
     */
    __NV_SILENCE_DEPRECATION_BEGIN
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3(const double4 f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_ue5m3>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_ue5m3>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }
    __NV_SILENCE_DEPRECATION_END
    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_16a vector data type,
     * relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_double_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3(const double4_16a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_ue5m3>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_ue5m3>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Constructor from \p double4_32a vector data type,
     * relies on \p __NV_SATFINITE
     * behavior for large input values and \p cudaRoundNearest for
     * rounding.
     * \see __nv_cvt_double_to_ue5m3 for further details
     */
    explicit __CUDA_HOSTDEVICE_FP8__ __nv_fp8x4_ue5m3(const double4_32a f) {
        const double2 flo = {f.x, f.y};
        const double2 fhi = {f.z, f.w};
        const __nv_fp8x2_storage_t rlo = static_cast<__nv_fp8x2_ue5m3>(flo).__x;
        const __nv_fp8x2_storage_t rhi = static_cast<__nv_fp8x2_ue5m3>(fhi).__x;
        __x = __internal_pack_u16x2_to_u32(rlo, rhi);
    }

#if !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__)
    /* Widening converts */

    /**
     * \ingroup CUDA_MATH_FP8_MISC
     * Conversion operator to \p float4 vector data type.
     */
    explicit __CUDA_HOSTDEVICE_FP8__ operator float4() const {
        __nv_fp8x2_ue5m3 lo;
        lo.__x = static_cast<__nv_fp8x2_storage_t>(__x);
        __nv_fp8x2_ue5m3 hi;
        hi.__x = static_cast<__nv_fp8x2_storage_t>(__x >> 16U);

        float2 rlo = static_cast<float2>(lo);
        float2 rhi = static_cast<float2>(hi);
        float4 res = {rlo.x, rlo.y, rhi.x, rhi.y};
        return res;
    }
#endif /* !defined(__CUDA_NO_FP8_CONVERSION_OPERATORS__) */
#endif /* !defined(__CUDA_NO_FP8_CONVERSIONS__) */
};

#undef __NV_FP8_SM_90_TL__
#undef __NV_FP8_SM_90_TL_BUILTIN__
#undef __NV_FP8_SM_100_TL__
#undef __NV_FP8_SM_100_TL_BUILTIN__
#undef __NV_FP8_SM_107F_TL__
#undef __NV_FP8_SM_107F_TL_BUILTIN__

#endif /* defined(__cplusplus) */

#endif /* end of include guard: __CUDA_FP8_HPP__ */
