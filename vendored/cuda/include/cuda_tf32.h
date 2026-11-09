/*
 * Copyright 2025 NVIDIA Corporation.  All rights reserved.
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

#ifndef __CUDA_TF32_H__
#define __CUDA_TF32_H__

#ifndef __NV_TL__
#define __NV_TL__
#endif

#ifndef __NV_TL_BUILTIN__
#define __NV_TL_BUILTIN__
#endif

#if !defined(_MSC_VER) && __cplusplus >= 201103L
#  define __CPP_VERSION_AT_LEAST_11_TF32
#elif _MSC_FULL_VER >= 190024210 && _MSVC_LANG >= 201103L
#  define __CPP_VERSION_AT_LEAST_11_TF32
#endif

#if defined(__CUDACC__)
#  define __CUDA_ALIGN__(align) __align__(align)
#else
#  if defined(__CPP_VERSION_AT_LEAST_11_TF32)
#    define __CUDA_ALIGN__(n) alignas(n)
#  else /* defined(__CPP_VERSION_AT_LEAST_11_TF32)*/
#    if defined(__GNUC__)
#      define __CUDA_ALIGN__(n) __attribute__ ((aligned(n)))
#    elif defined(_MSC_VER)
#      define __CUDA_ALIGN__(n) __declspec(align(n))
#    else
#      define __CUDA_ALIGN__(n)
#    endif /* defined(__GNUC__) */
#  endif /* defined(__CPP_VERSION_AT_LEAST_11_TF32) */
#endif /* defined(__CUDACC__) */

#if defined(__cplusplus)

#define __CUDA_TF32_TYPES_EXIST__
struct __half;
struct __nv_bfloat16;

#if (defined(__CUDACC_RTC__) && ((__CUDACC_VER_MAJOR__ > 12) || ((__CUDACC_VER_MAJOR__ == 12) && (__CUDACC_VER_MINOR__ >= 3))))
#  define __CUDA_HOSTDEVICETILE__ __device__ __NV_TL__
#elif defined(__CUDACC__) || defined(_NVHPC_CUDA)
#  define __CUDA_HOSTDEVICETILE__ __host__ __device__ __NV_TL__
#else /* defined(__CUDACC__) || defined(_NVHPC_CUDA) */
#  define __CUDA_HOSTDEVICETILE__
#endif /* (defined(__CUDACC_RTC__) && ((__CUDACC_VER_MAJOR__ > 12) || ((__CUDACC_VER_MAJOR__ == 12) && (__CUDACC_VER_MINOR__ >= 3))))) */

/** \cond NV_NSR */

/**
 * \brief __nv_tf32 datatype
 *
 * \details This structure implements the datatype for storing Tensor
 * Float 32 numbers. This format uses 19 bits to represent
 * its value: 1 sign bit, 8 bits for the exponent, and 10 bits of
 * precision for the mantissa. The storage size of this type is 32 bits
 * in total and it requires a 4-byte alignment. The arrangement of the
 * value bits within the 32 bits of storage is unspecified.
 *
 * This type is implemented in the \p cuda_tf32.h header.
 * Currently, the \p __nv_tf32 type may only be used in tile code.
 * It provides scalar constructors and conversion operators for basic
 * numeric types. \p __nv_tf32 may also be used as the element of a tile
 * (e.g. \p cuda::tiles::tile<__nv_tf32, cuda::tiles::shape<4>>) and passed to
 * CUDA Tile C++ APIs. The supported CUDA Tile C++ APIs include
 * conversions to other numeric types, memory operations, and matrix multiplication.
 * For details on these operations, please see the CUDA Tile C++ API Reference.
 */
struct __NV_TL_BUILTIN__ __CUDA_ALIGN__(4) __nv_tf32 {
public:

  /**
   * \brief Constructor by default.
   * \details Empty default constructor, result is uninitialized.
   */
#if defined(__CPP_VERSION_AT_LEAST_11_TF32)
  __nv_tf32() = default;
#else
  __NV_TL__ __nv_tf32() {}
#endif

  /**
   * Construct \p __nv_tf32 from \p float input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(float x);
  /**
   * Construct \p __nv_tf32 from \p __half input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(__half x);
  /**
   * Construct \p __nv_tf32 from \p __nv_bfloat16 input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(__nv_bfloat16 x);
  /**
   * Construct \p __nv_tf32 from \p double input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(double x);
  /**
   * Construct \p __nv_tf32 from \p unsigned \p short \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(unsigned short int x);
  /**
   * Construct \p __nv_tf32 from \p unsigned \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(unsigned int x);
  /**
   * Construct \p __nv_tf32 from \p unsigned \p long \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(unsigned long int x);
  /**
   * Construct \p __nv_tf32 from \p unsigned \p long \p long \p int input using
   * default round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(unsigned long long int x);
  /**
   * Construct \p __nv_tf32 from \p short \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(short int x);
  /**
   * Construct \p __nv_tf32 from \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(int x);
  /**
   * Construct \p __nv_tf32 from \p long \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(long int x);
  /**
   * Construct \p __nv_tf32 from \p long \p long \p int input using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit __nv_tf32(long long int x);

  /**
   * Conversion operator to \p float data type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator float() const;
  /**
   * Conversion operator to \p __half data type using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator __half() const;
  /**
   * Conversion operator to \p __nv_bfloat16 data type using default
   * round-to-nearest-even rounding mode.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator __nv_bfloat16() const;
  /**
   * Conversion operator to \p double data type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator double() const;
  /**
   * Conversion operator to \p signed \p char data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator signed char() const;
  /**
   * Conversion operator to \p unsigned \p char data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator unsigned char() const;
  /**
   * Conversion operator to an implementation-defined \p char data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator char() const;
  /**
   * Conversion operator to \p short \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator short int() const;
  /**
   * Conversion operator to \p unsigned \p short \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator unsigned short int() const;
  /**
   * Conversion operator to \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator int() const;
  /**
   * Conversion operator to \p unsigned \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator unsigned int() const;
  /**
   * Conversion operator to \p long \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator long int() const;
  /**
   * Conversion operator to \p unsigned \p long \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator unsigned long int() const;
  /**
   * Conversion operator to \p long \p long \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator long long int() const;
  /**
   * Conversion operator to \p unsigned \p long \p long \p int data type.
   * Finite inputs are truncated toward zero. Behavior is undefined for NaN,
   * infinities, and finite inputs whose result after truncation toward zero is
   * outside the representable range of the destination type.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator unsigned long long int() const;
  /**
   * Conversion operator to \p bool data type.
   * +0 and -0 inputs convert to \p false.
   * Non-zero inputs, including NaN, convert to \p true.
   */
  __NV_TL__ __NV_TL_BUILTIN__ explicit operator bool() const;

private:
  unsigned int __x;
};

/** \endcond */

#undef __CUDA_HOSTDEVICETILE__
#endif /* defined(__cplusplus) */

#undef __CUDA_ALIGN__
#undef __CPP_VERSION_AT_LEAST_11_TF32
#endif /* __CUDA_TF32_H__ */
