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


#ifndef __CUDA_TILE__
#define __CUDA_TILE__

#ifndef __cpp_concepts
#error "This file needs C++20 features. Please compile with c++20 or later dialect"
#endif

#ifndef __CUDACC_RTC__
# include <cstdint>
# include <type_traits>

#ifdef __IRANGE_RUNTIME_CHECKS__
# include <cassert>
#endif
#endif

#if defined(__GNUC__) || defined(__CUDACC_RTC__)
# define __applicable_tile_hints__(...) __attribute__((applicable_tile_hints(__VA_ARGS__)))
#elif defined(_MSC_VER)
# define __applicable_tile_hints__(...) __declspec(__applicable_tile_hints__(__VA_ARGS__))
#endif

#if defined(_MSC_VER)
# define _CUTILE_DECLSPEC_EMPTY_BASES __declspec(empty_bases)
#else
# define _CUTILE_DECLSPEC_EMPTY_BASES
#endif

#define _CUTILE_BEGIN_CUDA_TILES_NAMESPACE namespace cuda::tiles::inline __1 {
#define _CUTILE_END_CUDA_TILES_NAMESPACE }

struct __half;
struct __nv_bfloat16;
struct __nv_fp8_e5m2;
struct __nv_fp8_e4m3;
struct __nv_fp8_e8m0;
struct __nv_fp8_ue5m3;
struct __nv_fp4_e2m1;
struct __nv_tf32;

_CUTILE_BEGIN_CUDA_TILES_NAMESPACE
    namespace detail {

#ifdef __CUDACC_RTC__
      template <typename T>
      constexpr bool is_class_v = __is_class(T);

      template <typename T, typename U>
      constexpr bool is_convertible_v = __is_convertible_to(T, U);
#else  /* !__CUDACC_RTC__ */
      template <typename T>
      constexpr bool is_class_v =  ::std::is_class_v<T>;

      template <typename T, typename U>
      constexpr bool is_convertible_v = ::std::is_convertible_v<T,U>;
#endif  /* __CUDACC_RTC__ */

      // Define TILE_CHAR_BIT locally to avoid dependency on <climits>
      // Following EDG's approach: 8 bits per byte is universal on all modern systems
      constexpr inline int TILE_CHAR_BIT = 8;
      
      // CV-qualifier removal
      template<typename T> struct remove_const { using type = T; };
      template<typename T> struct remove_const<const T> { using type = T; };
      template<typename T> using remove_const_t = typename remove_const<T>::type;

      template<typename T> struct remove_volatile { using type = T; };
      template<typename T> struct remove_volatile<volatile T> { using type = T; };
      template<typename T> using remove_volatile_t = typename remove_volatile<T>::type;

      template<typename T> struct remove_cv { 
        using type = remove_volatile_t<remove_const_t<T>>; 
      };
      template<typename T> using remove_cv_t = typename remove_cv<T>::type;

      template<typename T> constexpr inline bool is_const_v = false;
      template<typename T> constexpr inline bool is_const_v<T const> = true;

      template<typename T> constexpr inline bool is_volatile_v = false;
      template<typename T> constexpr inline bool is_volatile_v<T volatile> = true;

      // Check if type is integral - using variable template specializations for faster compilation
      template<typename T>
      constexpr inline bool is_integral_impl_v = false;
      
      // Specialize for all integral types
      template<> constexpr inline bool is_integral_impl_v<bool> = true;
      template<> constexpr inline bool is_integral_impl_v<char> = true;
      template<> constexpr inline bool is_integral_impl_v<signed char> = true;
      template<> constexpr inline bool is_integral_impl_v<unsigned char> = true;
      template<> constexpr inline bool is_integral_impl_v<short> = true;
      template<> constexpr inline bool is_integral_impl_v<unsigned short> = true;
      template<> constexpr inline bool is_integral_impl_v<int> = true;
      template<> constexpr inline bool is_integral_impl_v<unsigned int> = true;
      template<> constexpr inline bool is_integral_impl_v<long> = true;
      template<> constexpr inline bool is_integral_impl_v<unsigned long> = true;
      template<> constexpr inline bool is_integral_impl_v<long long> = true;
      template<> constexpr inline bool is_integral_impl_v<unsigned long long> = true;
      template<> constexpr inline bool is_integral_impl_v<wchar_t> = true;
      template<> constexpr inline bool is_integral_impl_v<char8_t> = true;
      template<> constexpr inline bool is_integral_impl_v<char16_t> = true;
      template<> constexpr inline bool is_integral_impl_v<char32_t> = true;
            
      // Handle CV-qualified types (const, volatile, const volatile) like std::is_integral
      template<typename T>
      constexpr inline bool is_integral_v = is_integral_impl_v<remove_cv_t<T>>;
      
      // Custom same_as implementation (avoiding std::) - using variable template specializations for faster compilation
      template<typename T, typename U>
      constexpr inline bool is_same_v = false;
      
      template<typename T>
      constexpr inline bool is_same_v<T, T> = true;
      
      // Minimal unsigned detection for uint64_t exclusion only
      template<typename T>
      constexpr inline bool is_unsigned_long_long_v = 
        is_same_v<remove_cv_t<T>, unsigned long long>;

      template<typename T, typename... Ts>
      __tile__ __host__ __device__ consteval bool is_value_one_of(T value, Ts... vs) {
        return ((value == vs) || ...);
      }

      template<typename T> constexpr inline bool is_signed_v = false;
      template<typename T> requires is_integral_v<T>
      constexpr inline bool is_signed_v<T> = T(-1) < T(0);

      template<size_t size> __tile__ __host__ __device__ constexpr size_t numeric_limits_max_signed() {
	return (size_t(1) << (size * 8 - 1)) - 1;
      }
      
      template<typename T> __tile__ __host__ __device__ constexpr T numeric_limits_max() {
	if constexpr(is_same_v<remove_cv_t<T>, bool>) {
	  return true;
	} else if constexpr(is_signed_v<T>) {
	  return T(numeric_limits_max_signed<sizeof(T)>());
	} else {
	  return T(-1);
	}
      }

      template<typename T> __tile__ __host__ __device__ constexpr T numeric_limits_min() {
	if constexpr(is_same_v<remove_cv_t<T>, bool>) {
	  return false;
	} else if constexpr(is_signed_v<T>) {
	  return T(size_t(1) << (sizeof(T) * 8 - 1));
	} else {
	  return T(0);
	}
      }
      
      // Custom common_type implementation 
      
      // Helper for conditional type selection
      template<bool B, typename T, typename F>
      struct conditional { using type = F; };
      
      template<typename T, typename F>
      struct conditional<true, T, F> { using type = T; };
      
      template<bool B, typename T, typename F>
      using conditional_t = typename conditional<B, T, F>::type;
      
      // Decay simulation (remove cv and references, array/function decay)
      template<typename T> struct remove_reference { using type = T; };
      template<typename T> struct remove_reference<T&> { using type = T; };
      template<typename T> struct remove_reference<T&&> { using type = T; };
      template<typename T> using remove_reference_t = typename remove_reference<T>::type;
      
      // add_rvalue_reference implementation for declval support
      template<typename T> struct add_rvalue_reference { using type = T&&; };
      template<typename T> struct add_rvalue_reference<T&> { using type = T&; };
      template<typename T> struct add_rvalue_reference<T&&> { using type = T&&; };
      template<> struct add_rvalue_reference<void> { using type = void; };
      template<> struct add_rvalue_reference<const void> { using type = const void; };
      template<> struct add_rvalue_reference<volatile void> { using type = volatile void; };
      template<> struct add_rvalue_reference<const volatile void> { using type = const volatile void; };
      template<typename T> using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;
      
      template<typename T>
      struct decay_impl {
        using U = remove_reference_t<T>;
        using type = remove_cv_t<U>;
      };
      template<typename T> using decay_t = typename decay_impl<T>::type;
      
      // declval implementation - declaration only, never actually called
      template<typename T>
      add_rvalue_reference_t<T>
      __host__ __device__ __tile__
      declval() noexcept;

      template<typename T, typename... Args> constexpr inline bool is_constructible_v = requires { T(declval<Args>()...); };

      __host__ __device__ __tile__ consteval bool is_pow2_basic(size_t val) {
          return (val > 0) && ((val & (val - 1)) == 0);
      }
      
      template<typename T>
      __host__ __device__ __tile__ consteval bool is_pow2(T x) {
	if constexpr (is_signed_v<T>) {
	  if (x < 0) {
	    return false;
	  } else {
	    return is_pow2_basic(size_t(x));
	  }
	} else {
	  return is_pow2_basic(size_t{x});
	}
      }

      template<typename T, typename U> concept _same_as = is_same_v<T, U>;
      template<typename T> constexpr inline bool is_cv_qualified_v = !is_same_v<T, remove_cv_t<T>>;
    }

    template<typename T, typename U>
    concept same_as = detail::_same_as<T, U> && detail::_same_as<U, T>;

    template<typename T>
    concept integral = detail::is_integral_v<T>;


namespace detail {
  template<size_t...> struct index_sequence { };

  //-- make_index_sequence --
  template<size_t Size, size_t... Tail>
  struct _make_index_sequence {
    using type = typename _make_index_sequence<Size - 1, Size - 1, Tail...>::type;
  };
  template<size_t... Tail>
  struct _make_index_sequence<0, Tail...> {
    using type = index_sequence<Tail...>;
  };
  template<size_t Size>
  using make_index_sequence = typename _make_index_sequence<Size>::type;
  //-- end make_index_sequence --

  //-- PackElement --
  template<size_t idx, auto Head, auto... Tail>
  struct pack_element {
    static constexpr decltype(Head) Value = pack_element<idx - 1, Tail...>::Value;
  };

  template<auto Head, auto... Tail>
  struct pack_element<0, Head, Tail...> {
    static constexpr decltype(Head) Value = Head;
  };

  template<size_t idx, auto... Pack>
  constexpr inline auto pack_element_v = pack_element<idx, Pack...>::Value;
  //-- end of pack_element_v --
} // namespace detail

#ifdef __CUDACC_RTC__
  using uint32_t = unsigned int;
  using int32_t = int;
#if _WIN64
  using int64_t = long long int;
  using uint64_t = unsigned long long int;
#else  /* !_WIN64 */
  using int64_t = long int;  
  using uint64_t = unsigned long int;
#endif  /* _WIN64 */

#else  /*!__CUDACC_RTC__ */
  using uint32_t = ::uint32_t;
  using int32_t = ::int32_t;
  using int64_t = ::int64_t;
  using uint64_t = ::uint64_t;
#endif  /* __CUDACC_RTC__ */

  template<integral auto Value>
  struct integral_constant {
    using type = integral_constant;
    using value_type = decltype(Value);

    static_assert(!detail::is_cv_qualified_v<value_type>,
           "In cuda::tiles::integral_constant<Value>, template argument Value may not be cv-qualified.");
    
    static constexpr value_type value = Value;

    __tile__ __host__ __device__ constexpr operator value_type() const noexcept {
      return Value;
    }
    __tile__ __host__ __device__ constexpr value_type operator()() const noexcept {
      return Value;
    }
  };

  namespace detail {
    __tile__ __host__ __device__ constexpr bool isdigit(char x) noexcept {
      return x >= '0' && x <= '0' + 9;
    }

    template<typename T, size_t N> struct array {
      __tile__ __host__ __device__ constexpr auto begin() const { return &data[0]; }
      __tile__ __host__ __device__ constexpr auto end() const { return &data[N]; }
      T data[N];
    };
    template<typename T> struct array<T, 0> { T data[1]; };

    template<size_t N>
    __tile__ __host__ __device__ constexpr size_t parse_integer(array<char, N> digits) noexcept {
      int64_t result{0};
      int64_t base{10};
      for(size_t idx = 0; idx != N; ++idx) {
	char digit = digits.data[idx];
	
	if (digit == '\'')
	  continue;

      	result *= base;
      	result += (digit - '0');
      }

      return size_t(result);
    }

    template<size_t x>
    __tile__ __host__ __device__ constexpr auto narrow_integral_constant() noexcept {
      static_assert(x <= numeric_limits_max<long long>(), "Unsupported integral constant literal. Value larger than std::numeric_limits<long long>::max().");
      
      if constexpr(x <= numeric_limits_max<int>()) {
	return int(x);
      } else if constexpr(x <= numeric_limits_max<long>()) {
	return long(x);
      } else if constexpr(x <= numeric_limits_max<long long>()) {
	return (long long)x;
      } else {
	return 0;
      }
    }

    template<char c, char...> constexpr inline char head_v = c;
  } // namespace detail
    

  namespace literals {
    template<char... digits>
    __tile__ __host__ __device__ constexpr auto operator ""_ic() noexcept {
      static_assert(((detail::isdigit(digits) || digits == '\'') && ...) &&
		    (detail::isdigit(detail::head_v<digits...>)) &&
		    (sizeof...(digits) == 1 || detail::head_v<digits...> != '0'),
"Unsupported integral constant literal. Only decimal integer literals are supported.");

      constexpr auto data = detail::array<char, sizeof...(digits)>{digits...};
      constexpr auto value = detail::parse_integer(data);
      return integral_constant<detail::narrow_integral_constant<value>()>{};
    }
  } // namespace literals

  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x + y> operator+(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x * y> operator*(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x - y> operator-(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x / y> operator/(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x % y> operator%(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }

  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x & y> operator&(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }

  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x | y> operator|(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x ^ y> operator^(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<x << y> operator<<(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x, auto y>
  __tile__ __host__ __device__ constexpr integral_constant<(x >> y)>  operator>>(integral_constant<x>, integral_constant<y>) noexcept {
    return {};
  }
  
  template<auto x>
  __tile__ __host__ __device__ constexpr integral_constant<~x> operator~(integral_constant<x>) noexcept {
    return {};
  }
  
  template<auto x>
  __tile__ __host__ __device__ constexpr integral_constant<+x>  operator+(integral_constant<x>) noexcept {
    return {};
  }
  
  template<auto x>
  __tile__ __host__ __device__ constexpr integral_constant<-x> operator-(integral_constant<x>) noexcept {
    return {};
  }

  enum __dynamic_extent_type : size_t {
    dynamic_extent = static_cast<size_t>(-1)
  };
    
  enum __dynamic_stride_type : size_t {
    dynamic_stride = static_cast<size_t>(-1)
  };

  enum __dynamic_offset_type : ptrdiff_t {
    dynamic_offset = ptrdiff_t(size_t(1) << (sizeof(ptrdiff_t) * 8 - 1))
  };

  namespace detail {

    
    //-- begin static_extents_only_v
    template<typename, typename>
    struct static_extents_only {
      static constexpr bool value = false;
    };

    template<typename T, size_t... Idx>
    struct static_extents_only<T, index_sequence<Idx...>> {
      static constexpr bool value = ((T::static_extent(Idx) != dynamic_extent) && ...);
    };

    template<typename T>
    constexpr inline bool static_extents_only_v = static_extents_only<T, make_index_sequence<T::rank()>>::value;
    //-- end static_extents_only_v
    
    template<typename T> constexpr inline bool is_copyable_v = requires (T& lval, T const& lval_const) {
       T(lval);
       T(lval_const);
       T(static_cast<T&&>(lval));
       lval = lval;
       lval = lval_const;
       lval = static_cast<T&&>(lval);
    };

    template<typename T>
    requires is_same_v<remove_cv_t<T>, void>
    constexpr inline bool is_copyable_v<T> = false;

    template<auto> constexpr inline bool constant_evaluate_v = true;
    
    template<typename T>
    constexpr inline bool is_extents_like_v =
      is_copyable_v<T>
      && requires {
          requires !is_cv_qualified_v<typename T::rank_type>;
          requires is_integral_v<typename T::rank_type>;
          requires !is_cv_qualified_v<typename T::index_type>;
          requires is_integral_v<typename T::index_type> && !is_same_v<typename T::index_type, bool>;
          { T::rank() } -> same_as<typename T::rank_type>;
          requires constant_evaluate_v<T::rank()>;
          { T::rank_dynamic() } -> same_as<typename T::rank_type>;
          requires constant_evaluate_v<T::rank_dynamic()>;
          { T::static_extent(declval<typename T::rank_type const&>()) } -> same_as<size_t>;
          { declval<T const&>().extent(declval<typename T::rank_type const&>()) } -> same_as<typename T::index_type>;
      };

    template<typename T>
    constexpr inline bool is_strides_like_v =
      is_copyable_v<T>
      && requires {
          requires !is_cv_qualified_v<typename T::rank_type>;
          requires is_integral_v<typename T::rank_type>;
          requires !is_cv_qualified_v<typename T::stride_type>;
          requires is_integral_v<typename T::stride_type> && !is_same_v<typename T::stride_type, bool>;
          { T::rank() } -> same_as<typename T::rank_type>;
          requires constant_evaluate_v<T::rank()>;
          { T::rank_dynamic() } -> same_as<typename T::rank_type>;
          requires constant_evaluate_v<T::rank_dynamic()>;
          { T::static_stride(declval<typename T::rank_type const&>()) } -> same_as<size_t>;
          { declval<T const&>().stride(declval<typename T::rank_type const&>()) } -> same_as<typename T::stride_type>;
      };

    template<typename T>
    constexpr inline bool is_offsets_like_v =
      is_copyable_v<T>
      && requires {
          requires !is_cv_qualified_v<typename T::rank_type>;
          requires is_integral_v<typename T::rank_type>;
          requires !is_cv_qualified_v<typename T::offset_type>;
          requires is_integral_v<typename T::offset_type> && !is_same_v<typename T::offset_type, bool>;
          { T::rank() } -> same_as<typename T::rank_type>;
          requires constant_evaluate_v<T::rank()>;
          { T::rank_dynamic() } -> same_as<typename T::rank_type>;
          requires constant_evaluate_v<T::rank_dynamic()>;
          { T::static_offset(declval<typename T::rank_type const&>()) } -> same_as<ptrdiff_t>;
          { declval<T const&>().offset(declval<typename T::rank_type const&>()) } -> same_as<typename T::offset_type>;
      };
    
  } // namespace detail

  template<typename T> concept extents_like = detail::is_extents_like_v<detail::remove_cv_t<T>>;
  template<typename T> concept strides_like = detail::is_strides_like_v<detail::remove_cv_t<T>>;
  template<typename T> concept shape_like = extents_like<T> && detail::static_extents_only_v<detail::remove_cv_t<T>>;
  template<typename T> concept offsets_like = detail::is_offsets_like_v<detail::remove_cv_t<T>>;

  template<offsets_like T, offsets_like U>
  __tile__ __host__ __device__ constexpr bool offsets_equal(T const& lhs, U const& rhs) noexcept {
    if constexpr(T::rank() == U::rank()) {
      return [&]<size_t... Seq>(detail::index_sequence<Seq...>){
        return ((lhs.offset(Seq) == rhs.offset(Seq)) && ...);
      }(detail::make_index_sequence<T::rank()>{});
    } else {
      return false;
    }
  }

  template<extents_like T, extents_like U>
  __tile__ __host__ __device__ constexpr bool extents_equal(T const& lhs, U const& rhs) noexcept {
    if constexpr(T::rank() == U::rank()) {
      return [&]<size_t... Seq>(detail::index_sequence<Seq...>){
        return ((lhs.extent(Seq) == rhs.extent(Seq)) && ...);
      }(detail::make_index_sequence<T::rank()>{});
    } else {
      return false;
    }
  }

  template<strides_like T, strides_like U>
  __tile__ __host__ __device__ constexpr bool strides_equal(T const& lhs, U const& rhs) noexcept {
    if constexpr(T::rank() == U::rank()) {
      return [&]<size_t... Seq>(detail::index_sequence<Seq...>){
        return ((lhs.stride(Seq) == rhs.stride(Seq)) && ...);
      }(detail::make_index_sequence<T::rank()>{});
    } else {
      return false;
    }
  }

  namespace detail {
    template<typename T> constexpr inline size_t extent_value_or_dynamic = dynamic_extent;
    template<auto extent> constexpr inline size_t extent_value_or_dynamic<integral_constant<extent>> = size_t(extent);

    template<typename T> constexpr inline size_t stride_value_or_dynamic = dynamic_stride;
    template<auto stride> constexpr inline size_t stride_value_or_dynamic<integral_constant<stride>> = size_t(stride);

    template<typename T> constexpr inline ptrdiff_t offset_value_or_dynamic = dynamic_offset;
    template<auto offset> constexpr inline ptrdiff_t offset_value_or_dynamic<integral_constant<offset>> = ptrdiff_t(offset);
    
    template<typename T, size_t Seq, size_t N>
    struct array_tuple {

      template<size_t Idx> __tile__ __host__ __device__ constexpr T get() const noexcept {
        if constexpr(Idx == Seq) {
          return car;
        } else {
          return cdr.template get<Idx>();
        }
      }
      
      template<size_t Idx> __tile__ __host__ __device__ constexpr void set(T val) noexcept {
        if constexpr(Idx == Seq) {
          car = val;
        } else {
          return cdr.template set<Idx>(val);
        }
      }

      array_tuple() = default;

      template<typename Arg, typename... Args>
      __tile__ __host__ __device__
      array_tuple(Arg arg, Args... args) : car{arg}, cdr{args...} { }
      
      T car = 0;
      array_tuple<T, Seq + 1, N> cdr;
    };

    template<typename T, size_t M, size_t N>
    requires (M == (N - 1))
    struct array_tuple<T, M, N> {
      template<size_t Idx> __tile__ __host__ __device__ constexpr T get() const noexcept {
        return car;
      }
      
      template<size_t Idx> __tile__ __host__ __device__ constexpr void set(T val) noexcept {
        car = val;
      }
      
      T car = 0;
    };

    template<typename R, typename I, typename... Ts>
    __tile__ __host__ __device__ constexpr R select_element(I idx, Ts... elements) {
      return [=]<size_t... Seq>(detail::index_sequence<Seq...>) {
        R result{0};

        ((result += ((idx == Seq) * elements)), ...);

        return result;
      }(detail::make_index_sequence<sizeof...(Ts)>{});
    }

    template<typename value_type, size_t N> struct dynamic_value_storage {

      dynamic_value_storage() = default;
      
      template<typename... Args>
      __tile__ __host__ __device__
      dynamic_value_storage(Args... args) : arr{args...} { }
      
      template<size_t Idx> __tile__ __host__ __device__ constexpr value_type get() const noexcept { return arr.template get<Idx>(); }
      template<size_t Idx> __tile__ __host__ __device__ constexpr void set(value_type val) noexcept { return arr.template set<Idx>(val); }

      template<size_t Seq, size_t DynamicSeq, typename T, T Sentinel, T... StaticValues>
      __tile__ __host__ __device__ constexpr value_type get_value_helper(auto idx) const noexcept {
        constexpr size_t rank = sizeof...(StaticValues);
        constexpr T staticValue = select_element<T>(Seq, StaticValues...);
        constexpr bool isDynamic = staticValue == Sentinel;
        
        if constexpr(Seq >= rank) {
          return 0;
        } else {
          value_type acc = get_value_helper<Seq + 1, DynamicSeq + isDynamic, T, Sentinel, StaticValues...>(idx);

          if constexpr(isDynamic) {
            acc += (Seq == idx) * this->template get<DynamicSeq>();
          } else {
            acc += (Seq == idx) * staticValue;
          }

          return acc;
        }
      }

      template<typename T, T Sentinel, T... StaticValues>
      __tile__ __host__ __device__ constexpr value_type get_value(auto idx) const noexcept {
        return get_value_helper<0, 0, T, Sentinel, StaticValues...>(idx);
      }

      template<size_t Seq, size_t DynamicSeq, typename T, T Sentinel, T... StaticValues>
      __tile__ __host__ __device__
      static constexpr void storage_from_values_helper(dynamic_value_storage& storage) { }
  
      template<size_t Seq, size_t DynamicSeq, typename T, T Sentinel, T... StaticValues, typename OtherHead, typename... OtherTail>
      __tile__ __host__ __device__
      static constexpr void storage_from_values_helper(dynamic_value_storage& storage, OtherHead head, OtherTail... tail) {
        constexpr bool isDynamic = select_element<T>(Seq, StaticValues...) == Sentinel;

        if constexpr (isDynamic) {
          storage.template set<DynamicSeq>(value_type(head));
        }

        storage_from_values_helper<Seq + 1, DynamicSeq + isDynamic, T, Sentinel, StaticValues...>(storage, tail...);
      }
  
      template<typename T, T Sentinel, T... StaticValues, typename... OtherValues>
      __tile__ __host__ __device__
      static constexpr dynamic_value_storage storage_from_values(OtherValues... other) {
        dynamic_value_storage storage{};
        storage_from_values_helper<0, 0, T, Sentinel, StaticValues...>(storage, other...);
        return storage;
      }

      array_tuple<value_type, 0, N> arr;
    };
  
    template<typename value_type> struct dynamic_value_storage<value_type, 0> {
      template<size_t> __tile__ __host__ __device__ constexpr value_type get() const noexcept { return 0; }
      template<size_t> __tile__ __host__ __device__ constexpr void set(value_type) noexcept { }

      template<typename T, T Sentinel, T... StaticValues>
      __tile__ __host__ __device__ constexpr value_type get_value(auto idx) const noexcept {
        return select_element<T>(idx, StaticValues...);
      }

      template<typename T, T Sentinel, T... StaticValues, typename... OtherValues>
      __tile__ __host__ __device__
      static constexpr dynamic_value_storage storage_from_values(OtherValues... other) { return {}; }
    };

    template<typename T, typename... Ts>
    __tile__ __host__ __device__ constexpr size_t count_equal(T sentinel, Ts... values) noexcept {
      size_t count{0};
      ((count += (values == sentinel)), ...);
      return count;
    }
  }

template<integral IndexType, size_t... Extents>
struct __tile_builtin__ _CUTILE_DECLSPEC_EMPTY_BASES extents
  : private detail::dynamic_value_storage<IndexType, detail::count_equal(dynamic_extent, Extents...)> {
  static_assert(!detail::is_cv_qualified_v<IndexType>,
                "In cuda::tiles::extents<IndexType, Extents...>, template argument IndexType may not be cv-qualified.");
  
  using index_type = IndexType;
  using rank_type = size_t;

  __tile__ __host__ __device__ static constexpr rank_type rank() noexcept {
    return sizeof...(Extents);
  }

  __tile__ __host__ __device__ static constexpr rank_type rank_dynamic() noexcept {
    return detail::count_equal(dynamic_extent, Extents...);
  }

  __tile__ __host__ __device__ static constexpr size_t static_extent(rank_type idx) noexcept {
    return detail::select_element<size_t>(idx, Extents...);
  }

  __tile__ __host__ __device__ constexpr index_type extent(rank_type idx) const noexcept {
    return this->template get_value<size_t, dynamic_extent, Extents...>(idx);
  }

 private:
  using storage_t = detail::dynamic_value_storage<IndexType, rank_dynamic()>;

 public:
  constexpr extents() = default;

  template<typename... OtherExtents>
  requires ((detail::is_convertible_v<OtherExtents, index_type> && ...)
            && (sizeof...(OtherExtents) == rank_dynamic())
            && (sizeof...(OtherExtents) != 0))
  __tile__ __host__ __device__ constexpr explicit extents(OtherExtents... exts) noexcept
    : storage_t{index_type(exts)...}
  {
  }
  
  template<typename... OtherExtents>
  requires ((detail::is_convertible_v<OtherExtents, index_type> && ...)
            && (sizeof...(OtherExtents) == rank())
            && (sizeof...(OtherExtents) != 0)
            && (rank() != rank_dynamic()))
  __tile__ __host__ __device__ constexpr explicit extents(OtherExtents... exts) noexcept
    : storage_t{storage_t::template storage_from_values<size_t, dynamic_extent, Extents...>(exts...)} {
  }
};

  template<typename... Ts>
  extents(Ts...) -> extents<uint32_t, detail::extent_value_or_dynamic<detail::remove_cv_t<Ts>>...>;

template<integral StrideType, size_t... Strides>
struct __tile_builtin__ _CUTILE_DECLSPEC_EMPTY_BASES strides
  : private detail::dynamic_value_storage<StrideType, detail::count_equal(dynamic_stride, Strides...)> {
  static_assert(!detail::is_cv_qualified_v<StrideType>,
                "In cuda::tiles::strides<StrideType, Strides...>, template argument StrideType may not be cv-qualified.");
  
  using stride_type = StrideType;
  using rank_type = size_t;

  __tile__ __host__ __device__ static constexpr rank_type rank() noexcept {
    return sizeof...(Strides);
  }

  __tile__ __host__ __device__ static constexpr rank_type rank_dynamic() noexcept {
    return detail::count_equal(dynamic_stride, Strides...);
  }

  __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type idx) noexcept {
    return detail::select_element<size_t>(idx, Strides...);
  }

  __tile__ __host__ __device__ constexpr stride_type stride(rank_type idx) const noexcept {
    return this->template get_value<size_t, dynamic_stride, Strides...>(idx);
  }

 private:
  using storage_t = detail::dynamic_value_storage<StrideType, rank_dynamic()>;

 public:
  constexpr strides() = default;

  template<typename... OtherStrides>
    requires ((detail::is_convertible_v<OtherStrides, stride_type> && ...)
      && (sizeof...(OtherStrides) == rank_dynamic())
      && (sizeof...(OtherStrides) != 0))
  __tile__ __host__ __device__ constexpr explicit strides(OtherStrides... strds) noexcept
    : storage_t{stride_type(strds)...} {
  }
  
  template<typename... OtherStrides>
    requires ((detail::is_convertible_v<OtherStrides, stride_type> && ...)
      && (sizeof...(OtherStrides) == rank())
      && (sizeof...(OtherStrides) != 0)
      && (rank() != rank_dynamic()))
  __tile__ __host__ __device__ constexpr explicit strides(OtherStrides... strds) noexcept
    : storage_t{storage_t::template storage_from_values<size_t, dynamic_stride, Strides...>(strds...)} {
  }
};

template<typename... Ts>
strides(Ts...) -> strides<uint32_t, detail::stride_value_or_dynamic<detail::remove_cv_t<Ts>>...>;

template<integral OffsetType, ptrdiff_t... Offsets>
struct __tile_builtin__ _CUTILE_DECLSPEC_EMPTY_BASES offsets
  : private detail::dynamic_value_storage<OffsetType, detail::count_equal(dynamic_offset, Offsets...)> {
  static_assert(!detail::is_cv_qualified_v<OffsetType>,
                "In cuda::tiles::offsets<OffsetType, Offsets...>, template argument OffsetType may not be cv-qualified.");
  
  using offset_type = OffsetType;
  using rank_type = size_t;

  __tile__ __host__ __device__ static constexpr rank_type rank() noexcept {
    return sizeof...(Offsets);
  }

  __tile__ __host__ __device__ static constexpr rank_type rank_dynamic() noexcept {
    return detail::count_equal(dynamic_offset, Offsets...);
  }

  __tile__ __host__ __device__ static constexpr ptrdiff_t static_offset(rank_type idx) noexcept {
    return detail::select_element<ptrdiff_t>(idx, Offsets...);
  }

  __tile__ __host__ __device__ constexpr offset_type offset(rank_type idx) const noexcept {
    return this->template get_value<ptrdiff_t, dynamic_offset, Offsets...>(idx);
  }

 private:
  using storage_t = detail::dynamic_value_storage<OffsetType, rank_dynamic()>;

 public:
  constexpr offsets() = default;

  template<typename... OtherOffsets>
    requires ((detail::is_convertible_v<OtherOffsets, offset_type> && ...)
      && (sizeof...(OtherOffsets) == rank_dynamic())
      && (sizeof...(OtherOffsets) != 0))
  __tile__ __host__ __device__ constexpr explicit offsets(OtherOffsets... offs) noexcept
    : storage_t{offset_type(offs)...} {
  }
  
  template<typename... OtherOffsets>
    requires ((detail::is_convertible_v<OtherOffsets, offset_type> && ...)
      && (sizeof...(OtherOffsets) == rank())
      && (sizeof...(OtherOffsets) != 0)
      && (rank() != rank_dynamic()))
  __tile__ __host__ __device__ constexpr explicit offsets(OtherOffsets... offs) noexcept
    : storage_t{storage_t::template storage_from_values<ptrdiff_t, dynamic_offset, Offsets...>(offs...)} {
  }
};

template<typename... Ts>
offsets(Ts...) -> offsets<int32_t, detail::offset_value_or_dynamic<detail::remove_cv_t<Ts>>...>;

template<offsets_like Lhs, offsets_like Rhs>
__tile__ __host__ __device__ constexpr bool operator==(Lhs const& lhs, Rhs const& rhs) noexcept {
  return offsets_equal(lhs, rhs);
}

  namespace detail {    
    //-- begin extents_specialization_v
    template<typename T> constexpr inline bool is_extents_specialization_v = false;
    template<typename T, size_t... idx> constexpr inline bool is_extents_specialization_v<extents<T, idx...>> = true;
    //-- end extents_specialization_v
    
  template<typename, typename, typename> constexpr inline bool has_same_static_extents_v = false;
  template<typename T, typename U, size_t... Seq>
  constexpr inline bool has_same_static_extents_v<T, U, index_sequence<Seq...>> =
    ((T::static_extent(Seq) == U::static_extent(Seq)) && ...);
    
  template<typename T, typename U> constexpr inline bool is_same_shape_v =
    (T::rank() == U::rank()) && has_same_static_extents_v<T, U, make_index_sequence<T::rank()>>;

} // namespace detail

  template<extents_like Lhs, extents_like Rhs>
  __tile__ __host__ __device__ constexpr bool operator==(Lhs const& lhs, Rhs const& rhs) noexcept {
    return extents_equal(lhs, rhs);
  }

  template<strides_like Lhs, strides_like Rhs>
  __tile__ __host__ __device__ constexpr bool operator==(Lhs const& lhs, Rhs const& rhs) noexcept {
    return strides_equal(lhs, rhs);
  }

  template<typename T, typename U> concept same_shape = shape_like<T> && shape_like<U> && detail::is_same_shape_v<detail::remove_cv_t<T>, detail::remove_cv_t<U>>;

  namespace detail {
    //--begin shape_size_impl_v
    template<typename, typename> constexpr inline size_t shape_size_impl_v = 0;
    template<typename T, size_t... Seq>
    constexpr inline size_t shape_size_impl_v<T, index_sequence<Seq...>> = (T::static_extent(Seq) * ... * 1);
    //--end shape_size_v
  } // namespace detail

  // TODO: This must validate that no overflow occurs when computing the size
  template<shape_like T>
  constexpr inline size_t shape_size_v = detail::shape_size_impl_v<T, detail::make_index_sequence<T::rank()>>;

  namespace detail {
    constexpr inline size_t max_tile_elements_per_dim = 16777216;
    constexpr inline size_t max_tile_elements = 16777216;

    template<typename, typename> constexpr inline bool is_shape_extents_tile_compatible_v = false;
    template<typename T, size_t... Seq>
    constexpr inline bool is_shape_extents_tile_compatible_v<T, index_sequence<Seq...>> =
      ((T::static_extent(Seq) <= max_tile_elements_per_dim) && ...) &&
      (is_pow2(T::static_extent(Seq)) && ...);

    template<typename T> constexpr inline bool is_tile_compatible_shape_v =
      is_same_v<typename T::index_type, uint32_t> &&
      is_shape_extents_tile_compatible_v<T, make_index_sequence<T::rank()>> &&
      shape_size_v<T> <= max_tile_elements &&
      is_extents_specialization_v<T>;
    
  } // namespace detail
  
  template<typename T> concept tile_shape = shape_like<T> && detail::is_tile_compatible_shape_v<detail::remove_cv_t<T>>;

  template<size_t... Idx> using shape = extents<uint32_t, Idx...>;

  namespace detail {
    template<typename T> constexpr inline bool integral_scalar_v = false;
    template<typename T>
    requires is_integral_v<T>
    constexpr inline bool integral_scalar_v<T> = sizeof(T) <= 8;
    
    template<typename T> constexpr inline bool basic_floating_point_scalar_v = false;
    template<> constexpr inline bool basic_floating_point_scalar_v<__nv_bfloat16> = true;
    template<> constexpr inline bool basic_floating_point_scalar_v<__half> = true;
    template<> constexpr inline bool basic_floating_point_scalar_v<float> = true;
    template<> constexpr inline bool basic_floating_point_scalar_v<double> = true;

    template<typename T> constexpr inline bool restricted_floating_point_scalar_v = false;
    template<> constexpr inline bool restricted_floating_point_scalar_v<__nv_fp8_e5m2> = true;
    template<> constexpr inline bool restricted_floating_point_scalar_v<__nv_fp8_e4m3> = true;
    template<> constexpr inline bool restricted_floating_point_scalar_v<__nv_fp8_ue5m3> = true;
    template<> constexpr inline bool restricted_floating_point_scalar_v<__nv_fp8_e8m0> = true;
    template<> constexpr inline bool restricted_floating_point_scalar_v<__nv_fp4_e2m1> = true;
    template<> constexpr inline bool restricted_floating_point_scalar_v<__nv_tf32> = true;
   
    template<typename T> constexpr inline bool floating_point_scalar_v = basic_floating_point_scalar_v<T> || restricted_floating_point_scalar_v<T>;
    template<typename T> constexpr inline bool arithmetic_scalar_v     = integral_scalar_v<T> || basic_floating_point_scalar_v<T>;
    template<typename T> constexpr inline bool numeric_scalar_v        = arithmetic_scalar_v<T> || restricted_floating_point_scalar_v<T>;
    
    template<typename T> constexpr inline bool pointer_scalar_v     = false;
    template<typename T> constexpr inline bool pointer_scalar_v<T*> = numeric_scalar_v<remove_cv_t<T>> || is_same_v<remove_cv_t<T>, void>;
    
    template<typename T> constexpr inline bool scalar_v             = pointer_scalar_v<T> || numeric_scalar_v<T>;
    template<typename T> constexpr inline bool scalar               = scalar_v<remove_cv_t<T>>;
  } // namespace detail

  template <typename E, typename S>
  requires detail::scalar<E> && tile_shape<S>
  class tile;

  namespace detail {
    template<typename T> struct tile_element { using type = T; };
    template<typename E, typename S> struct tile_element<tile<E, S>> { using type = E; };
    template<typename T> using tile_element_t = typename tile_element<remove_cv_t<T>>::type;

    template<typename T>             constexpr inline bool tile_like_v             = scalar_v<T>;
    template<typename E, typename S> constexpr inline bool tile_like_v<tile<E, S>> = true;

    template<typename T> constexpr inline bool integral_tile                  = integral_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool basic_floating_point_tile      = basic_floating_point_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool restricted_floating_point_tile = restricted_floating_point_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool floating_point_tile            = floating_point_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool arithmetic_tile                = arithmetic_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool numeric_tile                   = numeric_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool pointer_tile                   = pointer_scalar_v<tile_element_t<T>>;
    template<typename T> constexpr inline bool tile_like                      = tile_like_v<remove_cv_t<T>>;

  } // namespace detail

  template<typename T> concept tile_like                      = detail::tile_like<T>;
  template<typename T> concept pointer_tile                   = detail::pointer_tile<T> && tile_like<T>;
  template<typename T> concept numeric_tile                   = detail::numeric_tile<T> && tile_like<T>;
  template<typename T> concept arithmetic_tile                = detail::arithmetic_tile<T> && numeric_tile<T>;
  template<typename T> concept floating_point_tile            = detail::floating_point_tile<T> && numeric_tile<T>;
  template<typename T> concept integral_tile                  = detail::integral_tile<T> && arithmetic_tile<T>;
  template<typename T> concept basic_floating_point_tile      = detail::basic_floating_point_tile<T> && floating_point_tile<T> && arithmetic_tile<T>;
  template<typename T> concept restricted_floating_point_tile = detail::restricted_floating_point_tile<T> && floating_point_tile<T>;

  template<typename T> concept scalar                           = tile_like<T> && detail::scalar<T>;
  template<typename T> concept integral_scalar                  = integral_tile<T> && scalar<T> && integral<T>;
  template<typename T> concept basic_floating_point_scalar      = basic_floating_point_tile<T> && scalar<T>;
  template<typename T> concept restricted_floating_point_scalar = restricted_floating_point_tile<T> && scalar<T>;
  template<typename T> concept floating_point_scalar            = floating_point_tile<T> && scalar<T>;
  template<typename T> concept arithmetic_scalar                = arithmetic_tile<T> && scalar<T>;
  template<typename T> concept numeric_scalar                   = numeric_tile<T> && scalar<T>;
  template<typename T> concept pointer_scalar                   = pointer_tile<T> && scalar<T>;

  template<tile_like T> using tile_element_t = detail::tile_element_t<T>;

  namespace detail {
    template<typename T> struct _tile_shape { using type = shape<>; };
    template<typename E, typename S> struct _tile_shape<tile<E,S>> { using type = S; };
  } // namespace detail

  template<tile_like T>
  using tile_shape_t = typename detail::_tile_shape<detail::remove_cv_t<T>>::type;

  template<tile_like T> constexpr inline size_t tile_rank_v = tile_shape_t<T>::rank();

  namespace detail {
    template<typename T, typename U>
    constexpr size_t
    __host__ __device__ __tile__
    min_rank() {
      size_t x(T::rank());
      size_t y(U::rank());
      return (x < y) ? x : y;
    }

    template<typename T, typename U>
    constexpr size_t 
    __host__ __device__ __tile__
    max_rank() {
      size_t x(T::rank());
      size_t y(U::rank());
      return (x < y) ? y : x;
    }

    constexpr inline size_t
    __host__ __device__ __tile__
     max(size_t x, size_t y) {
      return (x < y) ? y : x;
    }

    template<typename S1, typename S2, size_t dim>
    constexpr inline size_t 
    __host__ __device__ __tile__
    broadcast_extent() {
      // The caller guarantees that S1::rank() <= S2::rank()
      constexpr size_t minR = min_rank<S1, S2>();

      if (dim < (S2::rank() - minR)) {
	return S2::static_extent(dim);
      }

      size_t lhs = S1::static_extent(dim - (S2::rank() - minR));
      size_t rhs = S2::static_extent(dim);

      return lhs == 1 ? rhs : lhs;
    }
    
    //--begin shape_broadcast
    template<typename S1, typename S2, bool, typename Seq> struct shape_broadcast;
    template<typename S1, typename S2, size_t... Seq>
    struct shape_broadcast<S1, S2, true, index_sequence<Seq...>> {
      using type = shape<broadcast_extent<S1, S2, Seq>()...>;
    };
    template<typename S1, typename S2, size_t... Seq>
    struct shape_broadcast<S1, S2, false, index_sequence<Seq...>> {
      using type = shape<broadcast_extent<S2, S1, Seq>()...>;
    };
    //--end shape_broadcast

    constexpr bool 
    __host__ __device__ __tile__
    broadcast_compatible(size_t x, size_t y) {
      return x == y || x == 1 || y == 1;
    }

    template<typename T, typename U, typename Seq>
    constexpr inline bool broadcast_compatible_trailing_extents_v = false;

    template<typename T, typename U, size_t... Seq>
    constexpr inline bool broadcast_compatible_trailing_extents_v<T, U, index_sequence<Seq...>> =
      (broadcast_compatible(T::static_extent(T::rank() - Seq - 1), U::static_extent(U::rank() - Seq - 1)) && ...);
    
    template<typename T, typename U>
    constexpr inline bool shape_broadcast_compatible_v =
      broadcast_compatible_trailing_extents_v<T, U, make_index_sequence<min_rank<T, U>()>>;
  } // namespace detail

  template<typename T, typename U>
  concept shape_broadcast_compatible = shape_like<T> && shape_like<U> && detail::shape_broadcast_compatible_v<T, U>;

  template<shape_like S1, shape_like S2>
  requires shape_broadcast_compatible<S1, S2>
  using shape_broadcast_t = typename detail::shape_broadcast<S1, S2, S1::rank() <= S2::rank(), detail::make_index_sequence<detail::max_rank<S1, S2>()>>::type;

  namespace detail {
    constexpr bool 
    __host__ __device__ __tile__
    broadcastable_to(size_t x, size_t y) {
      return x == y || x == 1;
    }

    template<typename T, typename U, typename Seq>
    constexpr inline bool broadcastable_to_trailing_extents_v = false;

    template<typename T, typename U, size_t... Seq>
    constexpr inline bool broadcastable_to_trailing_extents_v<T, U, index_sequence<Seq...>> =
      (broadcastable_to(T::static_extent(T::rank() - Seq - 1), U::static_extent(U::rank() - Seq - 1)) && ...);
    
    template<typename T, typename U>
    constexpr inline bool shape_broadcastable_to_v = T::rank() <= U::rank() &&
      broadcastable_to_trailing_extents_v<T, U, make_index_sequence<min_rank<T, U>()>>;
  } // detail

  template<typename T, typename U> concept shape_broadcastable_to = shape_like<T> && shape_like<U> && detail::shape_broadcastable_to_v<T, U>;
  
  template<typename T, typename U>
  concept broadcastable_to = tile_like<T> && tile_shape<U> && shape_broadcastable_to<tile_shape_t<T>, U>;

  template<typename T, typename U>
  concept broadcast_compatible = tile_like<T> && tile_like<U> &&
				 shape_broadcast_compatible<tile_shape_t<T>, tile_shape_t<U>> &&
				 tile_shape<shape_broadcast_t<tile_shape_t<T>, tile_shape_t<U>>>;

  namespace detail {
    template<typename T> struct make_unsigned;
    template<> struct make_unsigned<bool> { using type = bool; };
    template<> struct make_unsigned<char> { using type = unsigned char; };
    template<> struct make_unsigned<signed char> { using type = unsigned char; };
    template<> struct make_unsigned<unsigned char> { using type = unsigned char; };
    template<> struct make_unsigned<char8_t> { using type = unsigned char; };
    template<> struct make_unsigned<short> { using type = unsigned short; };
    template<> struct make_unsigned<unsigned short> { using type = unsigned short; };
    template<> struct make_unsigned<char16_t> { using type = unsigned short; };
    template<> struct make_unsigned<int> { using type = unsigned int; };
    template<> struct make_unsigned<unsigned int> { using type = unsigned int; };
    template<> struct make_unsigned<char32_t> { using type = unsigned int; };
    template<> struct make_unsigned<long> { using type = unsigned long; };
    template<> struct make_unsigned<unsigned long> { using type = unsigned long; };
    template<> struct make_unsigned<long long> { using type = unsigned long long; };
    template<> struct make_unsigned<unsigned long long> { using type = unsigned long long; };
#ifndef _MSC_VER
    template<> struct make_unsigned<wchar_t> { using type = unsigned int; };
#else
    template<> struct make_unsigned<wchar_t> { using type = unsigned short; };
#endif

    template<typename T> using make_unsigned_t = typename make_unsigned<T>::type;

    template<typename T> constexpr inline size_t integral_conversion_rank_v{0};

    template<> constexpr inline size_t integral_conversion_rank_v<bool> = 0;
    template<> constexpr inline size_t integral_conversion_rank_v<char> = 1;
    template<> constexpr inline size_t integral_conversion_rank_v<signed char> = 1;
    template<> constexpr inline size_t integral_conversion_rank_v<unsigned char> = 1;
    template<> constexpr inline size_t integral_conversion_rank_v<char8_t> = 1;
    template<> constexpr inline size_t integral_conversion_rank_v<short> = 2;
    template<> constexpr inline size_t integral_conversion_rank_v<unsigned short> = 2;
    template<> constexpr inline size_t integral_conversion_rank_v<char16_t> = 2;
    template<> constexpr inline size_t integral_conversion_rank_v<int> = 3;
    template<> constexpr inline size_t integral_conversion_rank_v<unsigned int> = 3;
    template<> constexpr inline size_t integral_conversion_rank_v<char32_t> = 3;
    template<> constexpr inline size_t integral_conversion_rank_v<long> = 4;
    template<> constexpr inline size_t integral_conversion_rank_v<unsigned long> = 4;
    template<> constexpr inline size_t integral_conversion_rank_v<long long> = 5;
    template<> constexpr inline size_t integral_conversion_rank_v<unsigned long long> = 5;
#ifndef _MSC_VER
    template<> constexpr inline size_t integral_conversion_rank_v<wchar_t> = 3;
#else
    template<> constexpr inline size_t integral_conversion_rank_v<wchar_t> = 2;
#endif

    template<typename T> constexpr inline size_t integral_conversion_subrank_v{2};
    template<> constexpr inline bool integral_conversion_subrank_v<char8_t> = 1;
    template<> constexpr inline bool integral_conversion_subrank_v<char16_t> = 1;
    template<> constexpr inline bool integral_conversion_subrank_v<char32_t> = 1;
    template<> constexpr inline bool integral_conversion_subrank_v<char> = 0;
    template<> constexpr inline bool integral_conversion_subrank_v<wchar_t> = 0;
    
    template<typename T, typename U> struct integral_conversion_by_subrank;

    template<typename T, typename U>
    requires (integral_conversion_subrank_v<T> < integral_conversion_subrank_v<U>)
    struct integral_conversion_by_subrank<T, U> { using type = U; };

    template<typename T, typename U>
    requires (integral_conversion_subrank_v<T> > integral_conversion_subrank_v<U>)
    struct integral_conversion_by_subrank<T, U> { using type = T; };

    template<typename T, typename U>
    requires (is_same_v<T, U>)
    struct integral_conversion_by_subrank<T, U> { using type = T; };

    template<typename T, typename U> struct integral_conversion_by_rank;
    
    template<typename T, typename U>
    requires (integral_conversion_rank_v<T> < integral_conversion_rank_v<U>)
    struct integral_conversion_by_rank<T, U> { using type = U; };

    template<typename T, typename U>
    requires (integral_conversion_rank_v<T> > integral_conversion_rank_v<U>)
    struct integral_conversion_by_rank<T, U> { using type = T; };
    
    template<typename T, typename U>
    requires (integral_conversion_rank_v<T> == integral_conversion_rank_v<U>)
    struct integral_conversion_by_rank<T, U> { using type = typename integral_conversion_by_subrank<T, U>::type; };

    template<typename S, typename U> struct integral_conversion_by_mixed_signedness;

    template<typename S, typename U>
    requires (integral_conversion_rank_v<U> > integral_conversion_rank_v<S>)
    struct integral_conversion_by_mixed_signedness<S, U> { using type = U; };

    template<typename S, typename U>
    requires (integral_conversion_rank_v<S> >= integral_conversion_rank_v<U> && requires (U u) { S{u}; })
    struct integral_conversion_by_mixed_signedness<S, U> { using type = S; };

    template<typename S, typename U>
    requires (integral_conversion_rank_v<S> >= integral_conversion_rank_v<U> && !requires (U u) { S{u}; })
    struct integral_conversion_by_mixed_signedness<S, U> { using type = make_unsigned_t<S>; };
    
    template<typename T, typename U> struct integral_conversion;

    template<typename T, typename U>
    requires is_same_v<T, U>
    struct integral_conversion<T, U> { using type = T; };

    template<typename T, typename U>
    requires (!is_same_v<T, U> && is_signed_v<T> && !is_signed_v<U>)
    struct integral_conversion<T, U> { using type = typename integral_conversion_by_mixed_signedness<T, U>::type; };

    template<typename T, typename U>
    requires (!is_same_v<T, U> && !is_signed_v<T> && is_signed_v<U>)
    struct integral_conversion<T, U> { using type = typename integral_conversion_by_mixed_signedness<U, T>::type; };

    template<typename T, typename U>
    requires (!is_same_v<T, U> && is_signed_v<T> == is_signed_v<U>)
    struct integral_conversion<T, U> { using type = typename integral_conversion_by_rank<T, U>::type; };

    template<typename T> constexpr inline size_t floating_point_conversion_rank_v = 0;
    template<> constexpr inline size_t floating_point_conversion_rank_v<__half> = 0;
    template<> constexpr inline size_t floating_point_conversion_rank_v<__nv_bfloat16> = 0;
    template<> constexpr inline size_t floating_point_conversion_rank_v<float> = 1;
    template<> constexpr inline size_t floating_point_conversion_rank_v<double> = 2;

    template<typename T, typename U> struct floating_point_conversion_by_rank;
    
    template<typename T, typename U>
    requires (floating_point_conversion_rank_v<T> < floating_point_conversion_rank_v<U>)
    struct floating_point_conversion_by_rank<T, U> { using type = U; };

    template<typename T, typename U>
    requires (floating_point_conversion_rank_v<T> > floating_point_conversion_rank_v<U>)
    struct floating_point_conversion_by_rank<T, U> { using type = T; };

    template<typename T, typename U>
    requires (is_same_v<T, U>)
    struct floating_point_conversion_by_rank<T, U> { using type = T; };
    
    template<typename T, typename U> struct arithmetic_scalar_conversion;

    template<integral_scalar T, integral_scalar U> struct arithmetic_scalar_conversion<T, U> {
      using type = typename integral_conversion<T, U>::type;
    };

    template<floating_point_scalar T, floating_point_scalar U> struct arithmetic_scalar_conversion<T, U> {
      using type = typename floating_point_conversion_by_rank<T, U>::type;
    };
    
    template<integral_scalar T, floating_point_scalar U>
    struct arithmetic_scalar_conversion<T, U> { using type = U; };
    
    template<floating_point_scalar T, integral_scalar U>
    struct arithmetic_scalar_conversion<T, U> { using type = T; };

    template<typename T, typename U>
    using arithmetic_scalar_conversion_t = typename arithmetic_scalar_conversion<remove_cv_t<T>, remove_cv_t<U>>::type;

    template<typename T> constexpr inline bool is_bfloat16_v = is_same_v<remove_cv_t<T>, __nv_bfloat16>;
    template<typename T> constexpr inline bool is_float16_v = is_same_v<remove_cv_t<T>, __half>;
    template<typename T> constexpr inline bool is_float32_v = is_same_v<remove_cv_t<T>, float>;
    template<typename T> constexpr inline bool is_float64_v = is_same_v<remove_cv_t<T>, double>;

    template<typename T, typename U>
    constexpr inline bool floating_point_scalar_with_unordered_rank_v = false;

    template<floating_point_scalar T, floating_point_scalar U>
    constexpr inline bool floating_point_scalar_with_unordered_rank_v<T, U> =
      (floating_point_conversion_rank_v<T> == floating_point_conversion_rank_v<U>) && !is_same_v<T, U>;

    template<typename T, typename U> struct arithmetic_tile_conversion;

    template<typename T, typename U>
    requires (!detail::scalar<T> && detail::scalar<U>)
    struct arithmetic_tile_conversion<T, U> {
      using type = tile<tile_element_t<T>, shape_broadcast_t<tile_shape_t<T>, tile_shape_t<U>>>;
    };
    
    template<typename T, typename U>
    requires (detail::scalar<T> && !detail::scalar<U>)
    struct arithmetic_tile_conversion<T, U> {
      using type = tile<tile_element_t<U>, shape_broadcast_t<tile_shape_t<T>, tile_shape_t<U>>>;
    };

    template<typename T, typename U>
    requires (!detail::scalar<T> && !detail::scalar<U>)
    struct arithmetic_tile_conversion<T, U> {
      using type = tile<arithmetic_scalar_conversion_t<tile_element_t<T>, tile_element_t<U>>,
			shape_broadcast_t<tile_shape_t<T>, tile_shape_t<U>>>;
    };

    template<typename T, typename U>
    requires (detail::scalar<T> && detail::scalar<U>)
    struct arithmetic_tile_conversion<T, U> {
      using type = arithmetic_scalar_conversion_t<T, U>;
    };

    template<typename T, typename U>
    using arithmetic_tile_conversion_t = typename arithmetic_tile_conversion<remove_cv_t<T>, remove_cv_t<U>>::type;

    enum class fp_kind : unsigned char {
      e2m1, e5m2, e4m3, e8m0, ue5m3, fp16, bf16, tf32, fp32, fp64
    };

    template<typename T> constexpr inline fp_kind fp_kind_v{255};
    template<> constexpr inline fp_kind fp_kind_v<__nv_fp4_e2m1> = fp_kind::e2m1;
    template<> constexpr inline fp_kind fp_kind_v<__nv_fp8_e5m2> = fp_kind::e5m2;
    template<> constexpr inline fp_kind fp_kind_v<__nv_fp8_e4m3> = fp_kind::e4m3;
    template<> constexpr inline fp_kind fp_kind_v<__nv_fp8_e8m0> = fp_kind::e8m0;
    template<> constexpr inline fp_kind fp_kind_v<__nv_fp8_ue5m3> = fp_kind::ue5m3;
    template<> constexpr inline fp_kind fp_kind_v<__half> = fp_kind::fp16;
    template<> constexpr inline fp_kind fp_kind_v<__nv_bfloat16> = fp_kind::bf16;
    template<> constexpr inline fp_kind fp_kind_v<__nv_tf32> = fp_kind::tf32;
    template<> constexpr inline fp_kind fp_kind_v<float> = fp_kind::fp32;
    template<> constexpr inline fp_kind fp_kind_v<double> = fp_kind::fp64;
    
    __tile__ __host__ __device__ consteval auto fp_kinds() {
      using enum fp_kind;
      
      return array<fp_kind, 10>{e2m1, e5m2, e4m3, e8m0, ue5m3, fp16, bf16, tf32, fp32, fp64};
    }

    __tile__ __host__ __device__ consteval bool directly_ordered_conversion_rank(fp_kind x, fp_kind y) {
      using enum fp_kind;
      
      switch(x) {
      case e4m3:
        return y == fp16 || y == bf16;
      case e5m2:
        return y == fp16 || y == bf16;
      case e8m0:
        return y == bf16;
      case ue5m3:
        return y == bf16;
      case e2m1:
        return y == e4m3 || y == e5m2;
      case fp16:
        return y == tf32;
      case bf16:
        return y == tf32;
      case tf32:
        return y == fp32;
      case fp32:
        return y == fp64;
      case fp64:
        return false;
      }

      return false;
    }

    __tile__ __host__ __device__ consteval bool ordered_conversion_rank(fp_kind x, fp_kind y) {
      if (directly_ordered_conversion_rank(x, y)) {
        return true;
      }

      bool ordered = false;
      for (auto neighbor : fp_kinds()) {
        if (directly_ordered_conversion_rank(x, neighbor)) {
          ordered |= ordered_conversion_rank(neighbor, y);
        }
      }

      return ordered;
    }

    template<typename T, typename U> constexpr inline bool same_underlying_floating_point_type_v =
      floating_point_scalar<T> && floating_point_scalar<U> && (fp_kind_v<T> == fp_kind_v<U>);
    
    template<typename Src, typename Dst>
    constexpr inline bool narrowing_fp_conversion_v =
      !same_underlying_floating_point_type_v<Src, Dst> && !ordered_conversion_rank(fp_kind_v<Src>, fp_kind_v<Dst>);
      
    template<typename Src, typename Dst>
    constexpr inline bool narrowing_scalar_conversion_v =
      (floating_point_scalar<Src> && integral_scalar<Dst>) ||
      (floating_point_scalar<Src> && floating_point_scalar<Dst> && narrowing_fp_conversion_v<Src, Dst>) ||
      (integral_scalar<Src> && floating_point_scalar<Dst>) ||
      (integral_scalar<Src> && integral_scalar<Dst> && !requires (Src x) { Dst{x}; }) ||
      (pointer_scalar<Src> && same_as<Dst, bool>);

    template<typename T, typename U>
    constexpr inline bool non_int_to_float_narrowing_conversion_v =
      narrowing_scalar_conversion_v<T, U> && !(integral_scalar<T> && floating_point_scalar<U>);
  
    template<typename T, typename U>
    constexpr inline bool arithmetic_tile_data_type_convertible_v = false;
    
    template<typename T, typename U>
    requires (!floating_point_scalar_with_unordered_rank_v<tile_element_t<T>, tile_element_t<U>>)
    constexpr inline bool arithmetic_tile_data_type_convertible_v<T, U> = 
      !non_int_to_float_narrowing_conversion_v<tile_element_t<T>, tile_element_t<arithmetic_tile_conversion_t<T, U>>> &&
      !non_int_to_float_narrowing_conversion_v<tile_element_t<U>, tile_element_t<arithmetic_tile_conversion_t<T, U>>>;
  } // namespace detail


  template<typename T, typename U>
  concept arithmetic_tile_convertible = arithmetic_tile<T> &&
					arithmetic_tile<U> &&
					broadcast_compatible<T, U> &&
                                        detail::arithmetic_tile_data_type_convertible_v<T, U>;

  template<arithmetic_tile T, arithmetic_tile U>
  requires arithmetic_tile_convertible<T, U> 
  using arithmetic_tile_conversion_t = detail::arithmetic_tile_conversion_t<T, U>;

  namespace detail {
    template<typename T, typename U>
    struct arithmetic_tile_comparison {
      using type = tile<arithmetic_scalar_conversion_t<tile_element_t<T>, tile_element_t<U>>,
			shape_broadcast_t<tile_shape_t<T>, tile_shape_t<U>>>;
    };

    template<typename T, typename U>
    requires (detail::scalar<T> && detail::scalar<U>)
    struct arithmetic_tile_comparison<T, U> {
      using type = arithmetic_scalar_conversion_t<T, U>;
    };
  
    template<typename T, typename U>
    using arithmetic_tile_comparison_t = typename arithmetic_tile_comparison<remove_cv_t<T>, remove_cv_t<U>>::type;
  
    template<typename T, typename U>
    constexpr inline bool arithmetic_tile_data_type_comparable_v = false;

    template<typename T, typename U>
    requires (!floating_point_scalar_with_unordered_rank_v<tile_element_t<T>, tile_element_t<U>>)
    constexpr inline bool arithmetic_tile_data_type_comparable_v<T, U> = 
      !non_int_to_float_narrowing_conversion_v<tile_element_t<T>, tile_element_t<arithmetic_tile_comparison_t<T, U>>> &&
      !non_int_to_float_narrowing_conversion_v<tile_element_t<U>, tile_element_t<arithmetic_tile_comparison_t<T, U>>>;
  } // namespace detail

  template<typename T, typename U>
  concept arithmetic_tile_comparable = arithmetic_tile<T> &&
				       arithmetic_tile<U> &&
				       broadcast_compatible<T, U> &&
                                       detail::arithmetic_tile_data_type_comparable_v<T, U>;

  template<arithmetic_tile T, arithmetic_tile U>
  requires arithmetic_tile_comparable<T, U> 
  using arithmetic_tile_comparison_t = detail::arithmetic_tile_comparison_t<T, U>;

  namespace detail {
    template<typename T> constexpr inline bool is_pointer_v = false;
    template<typename T> constexpr inline bool is_pointer_v<T*> = true;

    template<typename T> struct remove_pointer { using type = T; };
    template<typename T> struct remove_pointer<T*> { using type = T; };
    template<typename T> using remove_pointer_t = typename remove_pointer<T>::type;

    template<typename Src, typename Dest>
    constexpr inline bool is_base_pointer_conversion_v = detail::is_class_v<remove_pointer_t<Src>> &&
							 detail::is_class_v<remove_pointer_t<Dest>> &&
							 !is_same_v<remove_cv_t<remove_pointer_t<Src>>,
								    remove_cv_t<remove_pointer_t<Dest>>>;

    template<typename Src, typename Dest>
    constexpr inline bool scalar_pointer_convertible_to_v =
      pointer_scalar<Src> && pointer_scalar<Dest> && requires() { Dest{Src{}}; } && !is_base_pointer_conversion_v<Src, Dest>;
    
    template<typename Src, typename Dest>
    constexpr inline bool scalar_convertible_to_v =
      (numeric_scalar<Src> && numeric_scalar<Dest>) ||
      (pointer_scalar<Src> && same_as<Dest, bool>) ||
      scalar_pointer_convertible_to_v<Src, Dest>;

    
  } // namespace detail
  
  template<typename T, typename U>
  concept scalar_convertible_to = scalar<T> && scalar<U> &&
				  detail::scalar_convertible_to_v<detail::remove_cv_t<T>,
								  detail::remove_cv_t<U>>;

  namespace detail {
    template<typename T, typename U>
    constexpr inline bool pointer_comparable_v = requires { T{} == U{}; };
  }
  
  template <typename T, typename U>
  concept pointer_comparable = pointer_scalar<T> && pointer_scalar<U> && detail::pointer_comparable_v<detail::remove_cv_t<T>, detail::remove_cv_t<U>>;

  template<typename T, typename U>
  concept non_narrowing_scalar_convertible_to = scalar_convertible_to<T, U> &&
						!detail::narrowing_scalar_conversion_v<detail::remove_cv_t<T>, detail::remove_cv_t<U>>;
  
  namespace detail {
    template<typename Tile, typename Element>
    struct tile_with_element {
      using type = tile<Element, tile_shape_t<Tile>>;
    };

    template<typename Tile, typename Element>
    requires detail::scalar<Tile>
    struct tile_with_element<Tile, Element> {
      using type = Element;
    };
  } // namespace detail
  
  template<tile_like Tile, scalar Element>
  using tile_with_element_t = typename detail::tile_with_element<detail::remove_cv_t<Tile>,
								 detail::remove_cv_t<Element>>::type;
  
  namespace detail {
    template<typename T> struct arithmetic_tile_promotion { using type = T; };

    template<typename T>
    requires detail::integral_tile<T>
    struct arithmetic_tile_promotion<T> {
      using type = tile_with_element_t<T, decltype(+declval<tile_element_t<T>>())>;
    };
  } // namespace detail
    
  template<arithmetic_tile T>
  using arithmetic_tile_promotion_t = typename detail::arithmetic_tile_promotion<detail::remove_cv_t<T>>::type;

  namespace detail {
    template<typename T, typename U, typename E>
    struct mutual_broadcast {
      using type = tile<E, shape_broadcast_t<tile_shape_t<T>, tile_shape_t<U>>>;
    };

    template<typename T, typename U, typename E>
    requires detail::scalar<T> && detail::scalar<U>
    struct mutual_broadcast<T, U, E> {
      using type = E;
    };
  } // namespace detail

  template<tile_like T, tile_like U, scalar E>
  requires broadcast_compatible<T, U>
  using mutual_broadcast_t = typename detail::mutual_broadcast<detail::remove_cv_t<T>,
							       detail::remove_cv_t<U>,
							       detail::remove_cv_t<E>>::type;

  template<typename T, typename U>
  concept tile_convertible_to = tile_like<T> && tile_like<U> 
				&& scalar_convertible_to<tile_element_t<T>, tile_element_t<U>>
				&& same_shape<tile_shape_t<T>, tile_shape_t<U>>;

  template<typename T>
  concept bool_tile_convertible = tile_like<T> && tile_convertible_to<T, tile_with_element_t<T, bool>>;

  template<typename T, typename U>
  concept non_narrowing_tile_convertible_to = tile_convertible_to<T, U> &&
					      non_narrowing_scalar_convertible_to<tile_element_t<T>, tile_element_t<U>>;

  template<typename T>
  concept loadable_tile = pointer_tile<T> && 
			  scalar<detail::remove_pointer_t<tile_element_t<T>>> && 
			  !detail::is_volatile_v<detail::remove_pointer_t<tile_element_t<T>>>;

  template<typename T>
  concept storeable_tile = loadable_tile<T> && !detail::is_const_v<detail::remove_pointer_t<tile_element_t<T>>>;

  template<loadable_tile T>
  using tile_load_t = tile_with_element_t<T, detail::remove_cv_t<detail::remove_pointer_t<tile_element_t<T>>>>;

    template <typename Element, typename Shape>
    requires detail::scalar<Element> && tile_shape<Shape>
    class 

      __tile_builtin__ alignas(alignof(Element)) tile  final {

      static_assert(!detail::is_cv_qualified_v<Element> && !detail::is_cv_qualified_v<Shape>,
           "In cuda::tiles::tile<E, S>, template arguments E and S may not be cv-qualified.");
        
      unsigned char buf[sizeof(Element) * shape_size_v<Shape>];

      // Bit-cast a flat, already-converted element array into the byte buffer.
      // Shared tail of the per-rank array constructors below.
      __tile__ consteval void store_element_array(
          const Element (&flat)[shape_size_v<Shape>]) noexcept {
        struct holder { unsigned char b[sizeof(buf)]; };
        holder h = __builtin_bit_cast(holder, flat);
        for (size_t i = 0; i < sizeof(buf); ++i)
          buf[i] = h.b[i];
      }
    public:
      using element_type = Element;
      using shape_type = Shape;
      using rank_type = typename shape_type::rank_type;
      using index_type = typename shape_type::index_type;

      tile() = default;

      // Construct a tile from a (possibly nested) braced list of compile-time
      // constant values matching the shape.

      // rank 1
      template<scalar SrcE, size_t N0>
      requires (shape_type::rank() == 1)
            && (N0 == shape_type::static_extent(0))
            && scalar_convertible_to<SrcE, element_type>
            && (!detail::is_pointer_v<element_type>)
      __tile__ consteval explicit(!non_narrowing_scalar_convertible_to<SrcE, element_type>)
      tile(const SrcE (&values)[N0]) noexcept {
        element_type tmp[shape_size_v<shape_type>];
        for (size_t i0 = 0; i0 < N0; ++i0)
          tmp[i0] = static_cast<element_type>(values[i0]);
        store_element_array(tmp);
      }

      // rank 2
      template<scalar SrcE, size_t N0, size_t N1>
      requires (shape_type::rank() == 2)
            && (N0 == shape_type::static_extent(0))
            && (N1 == shape_type::static_extent(1))
            && scalar_convertible_to<SrcE, element_type>
            && (!detail::is_pointer_v<element_type>)
      __tile__ consteval explicit(!non_narrowing_scalar_convertible_to<SrcE, element_type>)
      tile(const SrcE (&values)[N0][N1]) noexcept {
        element_type tmp[shape_size_v<shape_type>];
        for (size_t i0 = 0; i0 < N0; ++i0)
          for (size_t i1 = 0; i1 < N1; ++i1)
            tmp[i0 * N1 + i1] = static_cast<element_type>(values[i0][i1]);
        store_element_array(tmp);
      }

      // rank 3
      template<scalar SrcE, size_t N0, size_t N1, size_t N2>
      requires (shape_type::rank() == 3)
            && (N0 == shape_type::static_extent(0))
            && (N1 == shape_type::static_extent(1))
            && (N2 == shape_type::static_extent(2))
            && scalar_convertible_to<SrcE, element_type>
            && (!detail::is_pointer_v<element_type>)
      __tile__ consteval explicit(!non_narrowing_scalar_convertible_to<SrcE, element_type>)
      tile(const SrcE (&values)[N0][N1][N2]) noexcept {
        element_type tmp[shape_size_v<shape_type>];
        for (size_t i0 = 0; i0 < N0; ++i0)
          for (size_t i1 = 0; i1 < N1; ++i1)
            for (size_t i2 = 0; i2 < N2; ++i2)
              tmp[(i0 * N1 + i1) * N2 + i2] =
                  static_cast<element_type>(values[i0][i1][i2]);
        store_element_array(tmp);
      }

      // rank 4
      template<scalar SrcE, size_t N0, size_t N1, size_t N2, size_t N3>
      requires (shape_type::rank() == 4)
            && (N0 == shape_type::static_extent(0))
            && (N1 == shape_type::static_extent(1))
            && (N2 == shape_type::static_extent(2))
            && (N3 == shape_type::static_extent(3))
            && scalar_convertible_to<SrcE, element_type>
            && (!detail::is_pointer_v<element_type>)
      __tile__ consteval explicit(!non_narrowing_scalar_convertible_to<SrcE, element_type>)
      tile(const SrcE (&values)[N0][N1][N2][N3]) noexcept {
        element_type tmp[shape_size_v<shape_type>];
        for (size_t i0 = 0; i0 < N0; ++i0)
          for (size_t i1 = 0; i1 < N1; ++i1)
            for (size_t i2 = 0; i2 < N2; ++i2)
              for (size_t i3 = 0; i3 < N3; ++i3)
                tmp[((i0 * N1 + i1) * N2 + i2) * N3 + i3] =
                    static_cast<element_type>(values[i0][i1][i2][i3]);
        store_element_array(tmp);
      }

      // rank 5
      template<scalar SrcE, size_t N0, size_t N1, size_t N2, size_t N3, size_t N4>
      requires (shape_type::rank() == 5)
            && (N0 == shape_type::static_extent(0))
            && (N1 == shape_type::static_extent(1))
            && (N2 == shape_type::static_extent(2))
            && (N3 == shape_type::static_extent(3))
            && (N4 == shape_type::static_extent(4))
            && scalar_convertible_to<SrcE, element_type>
            && (!detail::is_pointer_v<element_type>)
      __tile__ consteval explicit(!non_narrowing_scalar_convertible_to<SrcE, element_type>)
      tile(const SrcE (&values)[N0][N1][N2][N3][N4]) noexcept {
        element_type tmp[shape_size_v<shape_type>];
        for (size_t i0 = 0; i0 < N0; ++i0)
          for (size_t i1 = 0; i1 < N1; ++i1)
            for (size_t i2 = 0; i2 < N2; ++i2)
              for (size_t i3 = 0; i3 < N3; ++i3)
                for (size_t i4 = 0; i4 < N4; ++i4)
                  tmp[(((i0 * N1 + i1) * N2 + i2) * N3 + i3) * N4 + i4] =
                      static_cast<element_type>(values[i0][i1][i2][i3][i4]);
        store_element_array(tmp);
      }

      template<tile_like Other>
      requires tile_convertible_to<Other, tile>
      __tile_builtin__ explicit(!non_narrowing_tile_convertible_to<Other, tile>) tile(Other) noexcept;
      
      template<scalar Other, typename = tile_with_element_t<tile, Other>>
      requires (shape_size_v<shape_type> == 1) && scalar_convertible_to<element_type, Other>
      __tile_builtin__ explicit operator Other() noexcept;
    };

    template<scalar E> tile(E) -> tile<detail::remove_cv_t<E>, shape<>>;

    // Deduction guides for the consteval array constructor: deduce the element
    // type and shape from a (possibly nested) C array argument.
    //   tile t{arr1d}; // arr1d is E[N0]        -> tile<E, shape<N0>>
    //   tile t{arr2d}; // arr2d is E[N0][N1]    -> tile<E, shape<N0, N1>>
    template<typename E, size_t N0>
    tile(const E (&)[N0]) -> tile<detail::remove_cv_t<E>, shape<N0>>;
    template<typename E, size_t N0, size_t N1>
    tile(const E (&)[N0][N1]) -> tile<detail::remove_cv_t<E>, shape<N0, N1>>;
    template<typename E, size_t N0, size_t N1, size_t N2>
    tile(const E (&)[N0][N1][N2]) -> tile<detail::remove_cv_t<E>, shape<N0, N1, N2>>;
    template<typename E, size_t N0, size_t N1, size_t N2, size_t N3>
    tile(const E (&)[N0][N1][N2][N3]) -> tile<detail::remove_cv_t<E>, shape<N0, N1, N2, N3>>;
    template<typename E, size_t N0, size_t N1, size_t N2, size_t N3, size_t N4>
    tile(const E (&)[N0][N1][N2][N3][N4]) -> tile<detail::remove_cv_t<E>, shape<N0, N1, N2, N3, N4>>;

    enum class __tile_builtin__ rounding_mode : unsigned char {
      round_ties_to_even = 0,
      round_toward_zero = 1,
      round_toward_negative = 2,
      round_toward_positive = 3,
      round_approximate = 4,
      round_full = 5,
      round_ties_to_away = 6,
    };

    namespace detail {
      template<rounding_mode Mode>
      constexpr inline bool rounding_mode_enumerator_v = (Mode <= rounding_mode::round_ties_to_away);
    } // namespace detail
    
    template<rounding_mode Value>
    requires detail::rounding_mode_enumerator_v<Value>
    struct rounding_mode_constant { 
      using type = rounding_mode_constant;
      using value_type = rounding_mode;
      static constexpr rounding_mode value = Value;

      __tile__ __host__ __device__ constexpr operator rounding_mode() const noexcept {
	return value;
      }
      __tile__ __host__ __device__ constexpr rounding_mode operator()() const noexcept {
        return value;
      }
    };

    __tile__ __host__ __device__ constexpr rounding_mode default_rounding_mode() noexcept {
       return rounding_mode::round_ties_to_even;
    }

    using round_ties_to_even_t = rounding_mode_constant<rounding_mode::round_ties_to_even>;
    using round_toward_zero_t = rounding_mode_constant<rounding_mode::round_toward_zero>;
    using round_toward_negative_t = rounding_mode_constant<rounding_mode::round_toward_negative>;
    using round_toward_positive_t = rounding_mode_constant<rounding_mode::round_toward_positive>;
    using round_approximate_t = rounding_mode_constant<rounding_mode::round_approximate>;
    using round_full_t = rounding_mode_constant<rounding_mode::round_full>;
    using round_ties_to_away_t = rounding_mode_constant<rounding_mode::round_ties_to_away>;
    using default_rounding_mode_t = rounding_mode_constant<default_rounding_mode()>;

    namespace detail {
      template<rounding_mode Mode>
      constexpr inline bool precise_rounding_mode_v = (Mode == rounding_mode::round_ties_to_even) ||
						      (Mode == rounding_mode::round_toward_zero) ||
						      (Mode == rounding_mode::round_toward_negative) ||
						      (Mode == rounding_mode::round_toward_positive);

      template<rounding_mode Mode,
               typename Src,
               typename Dst>
      __tile__ __device__ __host__
      consteval inline bool is_compatible_ftof_rounding_mode() {
        if (!floating_point_scalar_v<Src>
            || !floating_point_scalar_v<Dst>)
          return false;

        // Rounding to low-precision fp destination: only round_ties_to_even.
        if constexpr (is_same_v<Dst, __nv_fp8_e4m3> ||
                      is_same_v<Dst, __nv_fp8_e5m2> ||
                      is_same_v<Dst, __nv_fp4_e2m1> ||
                      is_same_v<Dst, __nv_fp8_ue5m3>)
          return Mode == rounding_mode::round_ties_to_even;

        if constexpr(is_same_v<Dst, __nv_fp8_e8m0>)
          return is_value_one_of(Mode, rounding_mode::round_toward_zero, rounding_mode::round_toward_positive);

        // Strict widening. Any IEEE float rounding mode is valid (no rounding actually occurs since
        // source is exactly representable).
        if constexpr (!narrowing_fp_conversion_v<Src, Dst>)
            return is_value_one_of(Mode,
                                   rounding_mode::round_ties_to_even,
                    	           rounding_mode::round_toward_zero,
                    	           rounding_mode::round_toward_negative,
                    	           rounding_mode::round_toward_positive,
                                   rounding_mode::round_ties_to_away);

        // f64 -> f32 narrowing.
        if (is_float64_v<Src> && is_float32_v<Dst>)
            return is_value_one_of(Mode,
                                   rounding_mode::round_ties_to_even,
         		                   rounding_mode::round_toward_zero,
         		                   rounding_mode::round_toward_negative,
         		                   rounding_mode::round_toward_positive);

        // f32 -> tf32 narrowing.
        if (is_float32_v<Src> && is_same_v<Dst, __nv_tf32>)
            return is_value_one_of(Mode,
                                   rounding_mode::round_ties_to_even,
                                   rounding_mode::round_toward_zero,
                                   rounding_mode::round_ties_to_away);

        // Other narrowing to standard types (half, bf16, etc.): round_ties_to_even and round_toward_zero.
        return is_value_one_of(Mode,
                               rounding_mode::round_ties_to_even,
                               rounding_mode::round_toward_zero);
      }

      // FIXME: relax constraints to support itof and ftoi.
      template<rounding_mode Mode, typename Src, typename Dst>
      constexpr inline bool compatible_element_cast_rounding_mode_v =
        detail::rounding_mode_enumerator_v<Mode>
        && is_compatible_ftof_rounding_mode<Mode, Src, Dst>();

      template<rounding_mode Mode, typename Tile>
      constexpr inline bool compatible_rounding_mode_v =
	(precise_rounding_mode_v<Mode> && basic_floating_point_tile<Tile>) ||
	(detail::rounding_mode_enumerator_v<Mode> && is_float32_v<tile_element_t<Tile>>);

      template<rounding_mode Mode, typename Tile>
      constexpr inline bool compatible_exp_tanh_rounding_mode_v =
	basic_floating_point_tile<Tile> &&
	(Mode == rounding_mode::round_full ||
	 (Mode == rounding_mode::round_approximate && is_float32_v<tile_element_t<Tile>>));
    } // namespace detail

    enum class __tile_builtin__ subnormals_rounding_mode : unsigned char {
      preserve_subnormals = 0,
      round_subnormals_to_zero = 1,
    };

    namespace detail {
      template<subnormals_rounding_mode Mode>
      constexpr inline bool subnormals_rounding_mode_enumerator_v = (Mode <= subnormals_rounding_mode::round_subnormals_to_zero);
    } // namespace detail
    
    template<subnormals_rounding_mode Value>
    requires detail::subnormals_rounding_mode_enumerator_v<Value>
    struct subnormals_rounding_mode_constant { 
      using type = subnormals_rounding_mode_constant;
      using value_type = subnormals_rounding_mode;
      static constexpr subnormals_rounding_mode value = Value;

      __tile__ __host__ __device__ constexpr operator subnormals_rounding_mode() const noexcept {
	return value;
      }
      __tile__ __host__ __device__ constexpr subnormals_rounding_mode operator()() const noexcept {
        return value;
      }
    };

    __tile__ __host__ __device__ constexpr subnormals_rounding_mode default_subnormals_rounding_mode() noexcept {
      return subnormals_rounding_mode::preserve_subnormals;
    }

    using preserve_subnormals_t = subnormals_rounding_mode_constant<subnormals_rounding_mode::preserve_subnormals>;
    using round_subnormals_to_zero_t = subnormals_rounding_mode_constant<subnormals_rounding_mode::round_subnormals_to_zero>;
    using default_subnormals_rounding_mode_t = subnormals_rounding_mode_constant<default_subnormals_rounding_mode()>;

    namespace detail {
      template<subnormals_rounding_mode Mode, typename T>
      constexpr inline bool compatible_subnormals_rounding_mode_v =
	(Mode == subnormals_rounding_mode::preserve_subnormals && basic_floating_point_tile<T>) ||
	(Mode == subnormals_rounding_mode::round_subnormals_to_zero && is_float32_v<tile_element_t<T>>);
    } // namespace detail

    enum class __tile_builtin__ saturation_mode : unsigned char { 
      saturate_none = 0,   // No saturation (if supported) 
      saturate_finite = 1, // Clamp to representable range 
    }; 

    namespace detail {
      template<saturation_mode Saturation>
      constexpr inline bool saturation_mode_enumerator_v = (Saturation <= saturation_mode::saturate_finite);
    } // namespace detail
    
    template<saturation_mode Value>
    requires detail::saturation_mode_enumerator_v<Value>
    struct saturation_mode_constant { 
      using type = saturation_mode_constant;
      using value_type = saturation_mode;
      static constexpr saturation_mode value = Value;

      __tile__ __host__ __device__ constexpr operator saturation_mode() const noexcept {
        return value;
      }
      __tile__ __host__ __device__ constexpr saturation_mode operator()() const noexcept {
        return value;
      }
    };

    using saturate_none_t   = saturation_mode_constant<saturation_mode::saturate_none>; 
    using saturate_finite_t = saturation_mode_constant<saturation_mode::saturate_finite>; 

    enum class __tile_builtin__ nan_propagation_mode : unsigned char {
      suppress_nan = 0,
      propagate_nan = 1
    };

    namespace detail {
      template<nan_propagation_mode Mode>
      constexpr inline bool nan_propagation_mode_enumerator_v = (Mode <= nan_propagation_mode::propagate_nan);
    } // namespace detail
    
    template<nan_propagation_mode Value>
    requires detail::nan_propagation_mode_enumerator_v<Value>
    struct nan_propagation_mode_constant { 
      using type = nan_propagation_mode_constant;
      using value_type = nan_propagation_mode;
      static constexpr nan_propagation_mode value = Value;

      __tile__ __host__ __device__ constexpr operator nan_propagation_mode() const noexcept {
        return value;
      }
      __tile__ __host__ __device__ constexpr nan_propagation_mode operator()() const noexcept {
        return value;
      }
    };

    __tile__ __host__ __device__ constexpr nan_propagation_mode default_nan_propagation_mode() noexcept {
      return nan_propagation_mode::suppress_nan;
    }

    using suppress_nan_t = nan_propagation_mode_constant<nan_propagation_mode::suppress_nan>;
    using propagate_nan_t = nan_propagation_mode_constant<nan_propagation_mode::propagate_nan>;
    using default_nan_propagation_mode_t = nan_propagation_mode_constant<default_nan_propagation_mode()>;

    namespace detail {
      template<nan_propagation_mode Nan, typename T>
      constexpr inline bool compatible_nan_propagation_mode_v = nan_propagation_mode_enumerator_v<Nan> && basic_floating_point_tile<T>;
    } // namespace detail
    
    enum class __tile_builtin__ memory_order : unsigned char {
      relaxed = 0,
      acquire = 1,
      release = 2,
      acq_rel = 3
    };

    namespace detail {
      template<memory_order Value> constexpr inline bool is_memory_order_enumerator_v = Value <= memory_order::acq_rel;
    } // namespace detail

    template<memory_order Value> 
    requires detail::is_memory_order_enumerator_v<Value>
    struct memory_order_constant {
      using type = memory_order_constant;
      using value_type = memory_order;
      
      static constexpr memory_order value = Value;
      __tile__ __host__ __device__ constexpr operator memory_order() const noexcept {
        return value;
      }
      __tile__ __host__ __device__ constexpr memory_order operator()() const noexcept {
         return value;
      }
    };

    using memory_order_relaxed_t = memory_order_constant<memory_order::relaxed>;
    using memory_order_acquire_t = memory_order_constant<memory_order::acquire>;
    using memory_order_release_t = memory_order_constant<memory_order::release>;
    using memory_order_acq_rel_t = memory_order_constant<memory_order::acq_rel>;

    namespace detail {
      template<memory_order Order> constexpr inline bool is_read_memory_order_v = Order == memory_order::relaxed || Order == memory_order::acquire;
      template<memory_order Order> constexpr inline bool is_write_memory_order_v = Order == memory_order::relaxed || Order == memory_order::release;
    } // namespace detail

    template<memory_order Order> concept read_memory_order = detail::is_read_memory_order_v<Order>;
    template<memory_order Order> concept write_memory_order = detail::is_write_memory_order_v<Order>;

    enum class __tile_builtin__ thread_scope : unsigned char {
      system = 0,
      device = 1,
      block = 2
    };

    namespace detail {
      template<thread_scope Value> constexpr inline bool is_thread_scope_enumerator_v = Value <= thread_scope::block;
    } // namespace detail

    template<thread_scope Value>
    requires  detail::is_thread_scope_enumerator_v<Value>
    struct thread_scope_constant {
      using type = thread_scope_constant;
      using value_type = thread_scope;
      static constexpr thread_scope value = Value;
      __tile__ __host__ __device__ constexpr operator thread_scope() const noexcept {
         return value;
      }
      __tile__ __host__ __device__ constexpr thread_scope operator()() const noexcept {
        return value;
      }
    };

    constexpr __tile__ __host__ __device__  thread_scope default_thread_scope() noexcept {
        return thread_scope::system;
    }
    
    using thread_scope_system_t = thread_scope_constant<thread_scope::system>;
    using thread_scope_device_t = thread_scope_constant<thread_scope::device>;
    using thread_scope_block_t = thread_scope_constant<thread_scope::block>;
    using default_thread_scope_t = thread_scope_constant<default_thread_scope()>;

    namespace detail {
      template<size_t N> constexpr bool
      __host__ __device__ __tile__
      is_permutation_array(array<size_t, N> arr) {
	if constexpr (N == 0) {
	  return true;
	} else {
	  bool visited[N] = {};
	  bool status = true;

	  for (size_t idx = 0; idx != N; ++idx) {
	    size_t value = arr.data[idx];

	    if (value >= N) {
	      status = false;
        continue;
	    }

	    if (visited[value]) {
	      status = false;
        continue;
	    }
	    visited[value] = true;
	  }

	  return status;
	}
      }
      
      template<size_t... Idx> constexpr bool 
      __host__ __device__ __tile__
      is_permutation() {
	return is_permutation_array(array<size_t, sizeof...(Idx)>{Idx...});
      }
    } // namespace detail
    
    template<size_t... Idx>
    requires (detail::is_permutation<Idx...>())
    struct __tile_builtin__ dimension_map {
      constexpr dimension_map() noexcept = default;

      template<typename... Ts> 
      requires (sizeof...(Ts) > 0)
      __tile__ __host__ __device__
      constexpr explicit dimension_map(Ts...) noexcept { }

      __tile__ __host__ __device__ static constexpr size_t rank() noexcept {
	return sizeof...(Idx);
      }
    
      __tile__ __host__ __device__ static constexpr size_t mapping(size_t n) noexcept {
         return detail::array<size_t, sizeof...(Idx)>{Idx...}.data[n];
     }
  };

   template<auto... Vs>
   requires (requires() { size_t{Vs}; } && ...)
   dimension_map(integral_constant<Vs>...) -> dimension_map<size_t{Vs}...>;

    namespace detail {
      template<typename T> constexpr inline bool is_dimension_map_v = false;
      template<size_t... Idx> constexpr inline bool is_dimension_map_v<dimension_map<Idx...>> = true;
    } // namespace detail

    namespace detail {
      template<typename, typename> struct make_extents_specialization_from_extents { };
      template<typename E, size_t... Seq> struct make_extents_specialization_from_extents<E, index_sequence<Seq...>> {
	using type = extents<typename E::index_type, E::static_extent(Seq)...>;
      };

      template<typename E> using make_extents_specialization_from_extents_t =
        typename make_extents_specialization_from_extents<E, make_index_sequence<E::rank()>>::type;
      
      template<typename S, typename T>
      constexpr inline bool assume_blocked_constraints_v =
	(shape_size_v<S> != 0) && S::rank() == tile_rank_v<T> && (pointer_tile<T> || integral_tile<T>);

      template<typename S, typename T>
      __tile_builtin__ T assume_blocked(T a) noexcept;
    } // namespace detail
    
    template<shape_like S, tile_like T>
    requires detail::assume_blocked_constraints_v<S, T>
    [[nodiscard]] __tile__ T assume_blocked(T a, S = {}) noexcept {
      return detail::assume_blocked<detail::make_extents_specialization_from_extents_t<S>>(a);
    }

    namespace detail {
      template<typename T>
      __tile__ __host__ __device__ constexpr int compare_integer_basic(T x, T y) noexcept {
	if (x < y) {
	  return -1;
	} else if (x == y) {
	  return 0;
	} else {
	  return 1;
	}
      }
      
      template<typename T, typename U>
      __tile__ __host__ __device__ constexpr int compare_integer(T x, U y) noexcept {
	if constexpr(is_signed_v<T> && is_signed_v<U>) {
	  return compare_integer_basic<int64_t>(int64_t{x}, int64_t{y});
	} else if constexpr(!is_signed_v<T> && !is_signed_v<U>) {
	  return compare_integer_basic<uint64_t>(uint64_t{x}, uint64_t{y});
	} else if constexpr(is_signed_v<T> && !is_signed_v<U>) {
	  if (int64_t{x} < int64_t{0}) {
	    return -1;
	  } else {
	    return compare_integer_basic<uint64_t>(uint64_t(x), uint64_t{y});
	  }
	} else if constexpr(!is_signed_v<T> && is_signed_v<U>) {
	  if (int64_t{y} < int64_t{0}) {
	    return 1;
	  } else {
	    return compare_integer_basic<uint64_t>(uint64_t{x}, uint64_t(y));
	  }
	}
      }
      
      template<auto LB, auto UB, typename E>
      constexpr inline bool assume_bounded_constraints_v =
	!is_same_v<E, bool> &&
	(compare_integer(LB, numeric_limits_min<E>()) >= 0) &&
	(compare_integer(UB, numeric_limits_max_signed<sizeof(E)>()) <= 0) &&
	(compare_integer(LB, UB) <= 0);

      template<auto UB, typename E>
      constexpr inline bool assume_bounded_above_constraints_v =
	!is_same_v<E, bool> &&
	(compare_integer(UB, numeric_limits_min<E>()) >= 0) &&
	(compare_integer(UB, numeric_limits_max_signed<sizeof(E)>()) <= 0);

      template<auto LB, typename E>
      constexpr inline bool assume_bounded_below_constraints_v =
	!is_same_v<E, bool> &&
	is_signed_v<E> &&
	(compare_integer(LB, numeric_limits_min<E>()) >= 0) &&
	(compare_integer(LB, numeric_limits_max<E>()) <= 0);
    } // namespace detail

    template<integral auto LB, integral auto UB, integral_tile T>
    requires detail::assume_bounded_constraints_v<LB, UB, tile_element_t<T>>
    [[nodiscard]] __tile_builtin__ T assume_bounded(T a, integral_constant<LB> = {}, integral_constant<UB> = {}) noexcept;

    template<integral auto UB, integral_tile T>
    requires detail::assume_bounded_above_constraints_v<UB, tile_element_t<T>>
    [[nodiscard]] __tile_builtin__ T assume_bounded_above(T a, integral_constant<UB> = {}) noexcept;

    template<integral auto LB, integral_tile T>
    requires detail::assume_bounded_below_constraints_v<LB, tile_element_t<T>>
    [[nodiscard]] __tile_builtin__ T assume_bounded_below(T a, integral_constant<LB> = {}) noexcept;
    
    template<integral auto Divisor, integral_tile T>
    requires (detail::is_pow2(Divisor))
    [[nodiscard]] __tile_builtin__ T assume_divisible(T a, integral_constant<Divisor> = {}) noexcept;

    template<integral auto Alignment, pointer_tile T>
    requires (detail::is_pow2(Alignment))
    [[nodiscard]] __tile_builtin__ T assume_aligned(T a, integral_constant<Alignment> = {}) noexcept;

    namespace detail {
      template<auto Divisor, auto Stride, auto Dim, typename T>
      constexpr inline bool assume_divisible_strided_constraints_v =
	detail::is_pow2(Divisor) &&
	(Stride > 0) &&
	(Dim >= 0) &&
	(Dim < tile_rank_v<T>) &&
	is_signed_v<tile_element_t<T>>;

      template<auto Alignment, auto Stride, auto Dim, typename T>
      constexpr inline bool assume_aligned_strided_constraints_v =
	detail::is_pow2(Alignment) &&
	(Stride > 0) &&
	(Dim >= 0) &&
	(Dim < tile_rank_v<T>);
    }
    
    template<integral auto Divisor, integral auto Stride, integral auto Dim, integral_tile T>
    requires detail::assume_divisible_strided_constraints_v<Divisor, Stride, Dim, T>
    [[nodiscard]] __tile_builtin__ T assume_divisible_strided(T a, integral_constant<Divisor> = {}, integral_constant<Stride> = {}, integral_constant<Dim> = {}) noexcept;

    template<integral auto Alignment, integral auto Stride, integral auto Dim, pointer_tile T>
    requires detail::assume_aligned_strided_constraints_v<Alignment, Stride, Dim, T>
    [[nodiscard]] __tile_builtin__ T assume_aligned_strided(T a, integral_constant<Alignment> = {}, integral_constant<Stride> = {}, integral_constant<Dim> = {}) noexcept;

    namespace detail {
      template <unsigned idx> __tile_builtin__ unsigned int bid() noexcept;
    } // namespace detail

    __tile__ inline uint3 bid() noexcept {
      return uint3{detail::bid<0>(), detail::bid<1>(), detail::bid<2>()};
    }

    namespace detail {
      template <unsigned idx> __tile_builtin__ unsigned int num_blocks();
    } // namespace detail

    __tile__ inline dim3 num_blocks() noexcept {
      return dim3{detail::num_blocks<0>(), detail::num_blocks<1>(), detail::num_blocks<2>()};
    }

    template<integral T> struct irange_iterator {
        static_assert(!detail::is_cv_qualified_v<T>, "In cuda::tiles::irange_iterator<T>, T may not be cv-qualified.");
      
        using difference_type = int64_t;
        using value_type = T;
        
        constexpr irange_iterator() noexcept = default;
        
        __host__ __device__ __tile__ constexpr irange_iterator(T current, T step) noexcept : current_(current), step_(step) { }

        __host__ __device__ __tile__ constexpr T operator*() const noexcept {
            return current_;
        }

        __host__ __device__ __tile__ constexpr irange_iterator& operator++() noexcept {
            current_ += step_;
            return *this;
        }

        __host__ __device__ __tile__ constexpr irange_iterator operator++(int) noexcept {
            irange_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        __host__ __device__ __tile__ constexpr bool operator==(irange_iterator const& other) const noexcept {
            return current_ == other.current_;
        }

        __host__ __device__ __tile__ constexpr bool operator==(T sentinel) const noexcept {
            return current_ >= sentinel;
        }

    private:
        T current_;
        T step_;
    };

    template<integral T> struct __tile_builtin__ irange {
        static_assert(!detail::is_cv_qualified_v<T>, "In cuda::tiles::irange<T>, T must not be cv-qualified.");

        using iterator = irange_iterator<T>;
        using const_iterator = irange_iterator<T>;

        __host__ __device__ __tile__ constexpr irange(T lb, T ub, T step = 1) noexcept
            : lowerBound_{lb}, 
              upperBound_{ub}, 
              step_{step} {
#ifdef __IRANGE_RUNTIME_CHECKS__
      assert(step_ > 0 && "In cuda::tiles::irange<T>(lb, ub, step), step must be positive.");
      assert(upperBound_ <= detail::numeric_limits_max<T>() - step + 1 && "In cuda::tiles::irange<T>(lb, ub, step): upperBound too large, may cause overflow.");
#endif
        }

        __host__ __device__ __tile__ constexpr iterator begin() const noexcept {
           return iterator{lowerBound_, step_};
        }

        __host__ __device__ __tile__ constexpr T end() const noexcept {
          return upperBound_;
        }

        __host__ __device__ __tile__ constexpr bool empty() const noexcept {
            return lowerBound_ >= upperBound_;
        }

        __host__ __device__ __tile__ constexpr T size() const noexcept {
            if (empty()) return T{0};
            using large_int_t = detail::conditional_t<detail::is_signed_v<T>, int64_t, uint64_t>;
            large_int_t difference = large_int_t{upperBound_} - large_int_t{lowerBound_};

#ifdef __IRANGE_RUNTIME_CHECKS__
            assert(uint64_t(difference) <= uint64_t(detail::numeric_limits_max<int64_t>()));
#endif
            return ((int64_t(difference) - 1) / step_) + 1;
        }

        __host__ __device__ __tile__ constexpr T lower_bound() const noexcept { return lowerBound_; }
        __host__ __device__ __tile__ constexpr T upper_bound() const noexcept { return upperBound_; }
        __host__ __device__ __tile__ constexpr T step() const noexcept { return step_; }

    private:
        T lowerBound_;
        T upperBound_;
        T step_;
    };

    template<typename U1, typename U2>
    irange(U1, U2) -> irange<arithmetic_tile_conversion_t<U1, U2>>;
    
    template<typename U1, typename U2, typename U3>
    irange(U1, U2, U3) -> irange<arithmetic_tile_conversion_t<arithmetic_tile_conversion_t<U1, U2>, U3>>;

    template <tile_like T>
    T __tile_builtin__ full(tile_element_t<T>) noexcept;

    template<numeric_tile T>
    requires (!detail::is_same_v<tile_element_t<T>, __nv_fp8_e8m0>)
    T __tile_builtin__ zeros() noexcept;

    template<numeric_tile T>
    T __tile_builtin__ ones() noexcept;

    template<floating_point_tile T>
    tile_with_element_t<T, bool> __tile_builtin__ isnan(T);

    template<floating_point_tile T>
    tile_with_element_t<T, bool> __tile_builtin__ isinf(T);

    namespace detail {
      template<typename T> constexpr inline bool is_object_ptr_v = pointer_tile<T> && !is_same_v<remove_cv_t<remove_pointer_t<tile_element_t<T>>>, void>;
    }

    // Pointer no-op
    template<pointer_tile Tile> __tile__ Tile operator+(Tile x) noexcept { return x; }

    // Pointer addition
    template<integral_tile Lhs, pointer_tile Rhs> 
    requires broadcast_compatible<Lhs, Rhs> && detail::is_object_ptr_v<Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, tile_element_t<Rhs>> operator+(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, integral_tile Rhs> 
    requires broadcast_compatible<Lhs, Rhs> && detail::is_object_ptr_v<Lhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, tile_element_t<Lhs>> 
    operator+(Lhs lhs, Rhs rhs) noexcept;

    // Pointer subtraction
    template<pointer_tile Lhs, integral_tile Rhs> 
    requires broadcast_compatible<Lhs, Rhs> && detail::is_object_ptr_v<Lhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, tile_element_t<Lhs>> 
    operator-(Lhs lhs, Rhs rhs) noexcept;

    // Pointer difference
    template<pointer_tile Lhs, pointer_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs> && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>> && detail::is_object_ptr_v<Lhs> && detail::is_object_ptr_v<Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, ptrdiff_t> operator-(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, pointer_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
              && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator==(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, pointer_tile Rhs> 
    requires broadcast_compatible<Lhs, Rhs> 
          && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>> 
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator!=(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, pointer_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
           && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator<(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, pointer_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
          && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator<=(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, pointer_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
          && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator>(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Lhs, pointer_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
          && pointer_comparable<tile_element_t<Lhs>, tile_element_t<Rhs>>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator>=(Lhs lhs, Rhs rhs) noexcept;

    template<pointer_tile Tile>
    __tile__ tile_with_element_t<Tile, bool> operator==(Tile lhs, decltype(nullptr)) noexcept {
      return lhs == tile_element_t<Tile>{nullptr};
    }

    template<pointer_tile Tile>
    __tile__ tile_with_element_t<Tile, bool> operator==(decltype(nullptr), Tile rhs) noexcept {
      return tile_element_t<Tile>{nullptr} == rhs;
    }

    template<pointer_tile Tile>
    __tile__ tile_with_element_t<Tile, bool> operator!=(Tile lhs, decltype(nullptr)) noexcept {
      return lhs != tile_element_t<Tile>{nullptr};
    }

    template<pointer_tile Tile>
    __tile__ tile_with_element_t<Tile, bool> operator!=(decltype(nullptr), Tile rhs) noexcept {
      return tile_element_t<Tile>{nullptr} != rhs;
    }

    /// Elementwise Addition
    template<arithmetic_tile Lhs, 
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs>
    operator+(Lhs lhs, Rhs rhs) noexcept;

    template<arithmetic_tile Lhs, 
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs>
    add(Lhs lhs, Rhs rhs) noexcept;

    template<rounding_mode Mode               = default_rounding_mode(),
	     subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	     arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs> 
    && detail::compatible_rounding_mode_v<Mode, arithmetic_tile_conversion_t<Lhs, Rhs>>
    && detail::compatible_subnormals_rounding_mode_v<SubMode, arithmetic_tile_conversion_t<Lhs, Rhs>>
    && detail::precise_rounding_mode_v<Mode>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs>
    add(Lhs lhs,
	Rhs rhs,
	rounding_mode_constant<Mode>, 
	subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    /// Elementwise integral promotion
    template<arithmetic_tile T>
    __tile_builtin__ arithmetic_tile_promotion_t<T> operator+(T x) noexcept;

    /// Elementwise multiplication
    template<arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    operator*(Lhs lhs, Rhs rhs) noexcept;

    template<arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    mul(Lhs lhs, Rhs rhs) noexcept; // 2

    template<rounding_mode Mode               = default_rounding_mode(),
	     subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	     arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs> 
    && detail::compatible_rounding_mode_v<Mode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
    && detail::compatible_subnormals_rounding_mode_v<SubMode, arithmetic_tile_conversion_t<Lhs, Rhs>>
    && detail::precise_rounding_mode_v<Mode>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    mul(Lhs lhs,
        Rhs rhs, 
	rounding_mode_constant<Mode>, 
	subnormals_rounding_mode_constant<SubMode> = {}) noexcept; 
    
    /// Elementwise subtraction
    template<arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    operator-(Lhs lhs, Rhs rhs) noexcept;

    template<arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    sub(Lhs lhs, Rhs rhs) noexcept; // 2

    template<rounding_mode Mode               = default_rounding_mode(),
	     subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	     arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs> 
          && detail::compatible_rounding_mode_v<Mode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
          && detail::compatible_subnormals_rounding_mode_v<SubMode, arithmetic_tile_conversion_t<Lhs, Rhs>>
          && detail::precise_rounding_mode_v<Mode>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    sub(Lhs lhs,
        Rhs rhs, 
	rounding_mode_constant<Mode>, 
	subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    /// Elementwise Negation
    template <arithmetic_tile T>
     __tile_builtin__ T operator-(T);

    /// Elementwise Division
    template<
	 arithmetic_tile Lhs, 
	 arithmetic_tile Rhs>
    requires 
         arithmetic_tile_convertible<Lhs, Rhs>
     __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> operator/(Lhs lhs, Rhs rhs) noexcept; 

    template<
	 arithmetic_tile Lhs, 
	 arithmetic_tile Rhs>
    requires 
         arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> div(Lhs lhs, Rhs rhs) noexcept;

    template<
	 rounding_mode Mode               = default_rounding_mode(), 
	 subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	 arithmetic_tile Lhs, 
	 arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs> 
    && detail::compatible_rounding_mode_v<Mode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
    && detail::compatible_subnormals_rounding_mode_v<SubMode, arithmetic_tile_conversion_t<Lhs, Rhs>>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> div(Lhs dividend, 
								    Rhs divisor, 
								    rounding_mode_constant<Mode>, 
								    subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    /// Elementwise Integer Remainder
    template<integral_tile Lhs, integral_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> operator%(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Remainder
    template<arithmetic_tile Lhs,
	     arithmetic_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> remainder(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Integer Division Rounded Towards Positive Infinity
    template<integral_tile Dividend, 
	     integral_tile Divisor>
    requires arithmetic_tile_convertible<Dividend, Divisor>
    __tile_builtin__ arithmetic_tile_conversion_t<Dividend, Divisor> 
    ceildiv(Dividend dividend, Divisor divisor) noexcept;

    /// Elementwise Integer Division Rounded Towards Negative Infinity
    template<integral_tile Dividend, 
	     integral_tile Divisor>
    requires arithmetic_tile_convertible<Dividend, Divisor>
    __tile_builtin__ arithmetic_tile_conversion_t<Dividend, Divisor> 
    floordiv(Dividend dividend, Divisor divisor) noexcept;

    /// Elementwise Strict Less Than Comparison
   template<arithmetic_tile Lhs, arithmetic_tile Rhs,
	    typename = arithmetic_tile_comparison_t<Lhs, Rhs>>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator<(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Less Than Or Equal
   template<arithmetic_tile Lhs, arithmetic_tile Rhs,
	    typename = arithmetic_tile_comparison_t<Lhs, Rhs>>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator<=(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Greater Than
   template<arithmetic_tile Lhs, arithmetic_tile Rhs,
	    typename = arithmetic_tile_comparison_t<Lhs, Rhs>>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator>(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Greater Than Or Equal
   template<arithmetic_tile Lhs, arithmetic_tile Rhs,
	    typename = arithmetic_tile_comparison_t<Lhs, Rhs>>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator>=(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Equality Comparison
   template<arithmetic_tile Lhs, arithmetic_tile Rhs,
	    typename = arithmetic_tile_comparison_t<Lhs, Rhs>>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator==(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Inequality Comparison
   template<arithmetic_tile Lhs, arithmetic_tile Rhs,
	    typename = arithmetic_tile_comparison_t<Lhs, Rhs>>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool> 
    operator!=(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Bitwise And
    template<integral_tile Lhs, integral_tile Rhs>
    requires arithmetic_tile_comparable<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> operator&(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Bitwise Or
    template<integral_tile Lhs, integral_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> operator|(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Bitwise Exclusive Or
    template<integral_tile Lhs, integral_tile Rhs>
    requires arithmetic_tile_convertible<Lhs, Rhs>
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> operator^(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Bitwise Complement
    template<integral_tile T>
    __tile_builtin__ T operator~(T x) noexcept;

    /// Elementwise Bitwise Left Shift
    template<integral_tile Lhs, integral_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, tile_element_t<Lhs>>
    operator<<(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Bitwise Right Shift
    template<integral_tile Lhs, integral_tile Rhs>
    requires broadcast_compatible<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, tile_element_t<Lhs>>
    operator>>(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Logical Conjunction
    template<bool_tile_convertible Lhs, bool_tile_convertible Rhs>
    requires broadcast_compatible<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator&&(Lhs lhs, Rhs rhs) noexcept;

    /// Elementwise Logical Disjunction
    template<bool_tile_convertible Lhs, bool_tile_convertible Rhs>
    requires broadcast_compatible<Lhs, Rhs>
    __tile_builtin__ mutual_broadcast_t<Lhs, Rhs, bool>
    operator||(Lhs lhs, Rhs rhs) noexcept;

    /// Logical Negation
    template<bool_tile_convertible T>
    __tile_builtin__ tile_with_element_t<T, bool> operator!(T x) noexcept;

    /// Elementwise Maximum
    template<arithmetic_tile Lhs, 
	     arithmetic_tile Rhs> 
    requires arithmetic_tile_convertible<Lhs, Rhs> 
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    max(Lhs lhs, Rhs rhs) noexcept;

    template<nan_propagation_mode NanMode = default_nan_propagation_mode(), 
	     subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(), 
	     arithmetic_tile Lhs, 
	     arithmetic_tile Rhs> 
    requires arithmetic_tile_convertible<Lhs, Rhs> 
         && detail::compatible_nan_propagation_mode_v<NanMode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
         && detail::compatible_subnormals_rounding_mode_v<SubMode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    max(Lhs lhs, Rhs rhs, 
	nan_propagation_mode_constant<NanMode>, 
	subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    /// Elementwise Minimum
    template<arithmetic_tile Lhs, 
	     arithmetic_tile Rhs> 
    requires arithmetic_tile_convertible<Lhs, Rhs> 
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    min(Lhs lhs, Rhs rhs) noexcept;

    template<nan_propagation_mode NanMode = default_nan_propagation_mode(), 
	     subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(), 
	     arithmetic_tile Lhs, 
	     arithmetic_tile Rhs> 
    requires arithmetic_tile_convertible<Lhs, Rhs> 
         && detail::compatible_nan_propagation_mode_v<NanMode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
         && detail::compatible_subnormals_rounding_mode_v<SubMode, arithmetic_tile_conversion_t<Lhs, Rhs>> 
    __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> 
    min(Lhs lhs, Rhs rhs, 
	nan_propagation_mode_constant<NanMode>, 
	subnormals_rounding_mode_constant<SubMode> = {}) noexcept;


    template<rounding_mode Mode = default_rounding_mode(), 
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(), 
             arithmetic_tile Lhs, 
             arithmetic_tile Rhs, 
             basic_floating_point_tile Acc> 
    requires broadcastable_to<Lhs, tile_shape_t<Acc>> 
          && broadcastable_to<Rhs, tile_shape_t<Acc>> 
          && non_narrowing_scalar_convertible_to<tile_element_t<Lhs>, tile_element_t<Acc>> 
          && non_narrowing_scalar_convertible_to<tile_element_t<Rhs>, tile_element_t<Acc>> 
          && detail::compatible_rounding_mode_v<Mode, Acc> 
          && detail::precise_rounding_mode_v<Mode> 
          && detail::compatible_subnormals_rounding_mode_v<SubMode, Acc> 
    __tile_builtin__ Acc fma(Lhs lhs, Rhs rhs, Acc acc, 
                             rounding_mode_constant<Mode> = {}, 
                             subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

   template<arithmetic_tile Tile> 
   __tile_builtin__ Tile abs(Tile x) noexcept;

   namespace detail {
     template<typename T> constexpr inline size_t bits_v = sizeof(T) * TILE_CHAR_BIT;
     template<> constexpr inline size_t bits_v<bool> = 1;
     template<> constexpr inline size_t bits_v<__nv_fp4_e2m1> = 4;
     template<> constexpr inline size_t bits_v<__nv_tf32> = 19;

     template<typename T> constexpr inline size_t bits_for_element_cast_v = bits_v<T>;
     template<> constexpr inline size_t bits_for_element_cast_v<bool> = 8;
     
     template<typename E, typename S>
     constexpr inline bool element_bitcast_constraints_v =
       (bits_for_element_cast_v<E> == bits_for_element_cast_v<tile_element_t<S>>);
   }

   template<scalar Element, tile_like Source>
   requires detail::element_bitcast_constraints_v<detail::remove_cv_t<Element>, Source>
   __tile_builtin__ tile_with_element_t<Source, Element> element_bitcast(Source x) noexcept;

   namespace detail {     
     template<typename E, size_t Dim, typename Src, typename Seq>
     struct repack_shape { };

     template<typename E, size_t Dim, typename Src, size_t... Seq>
     struct repack_shape<E, Dim, Src, index_sequence<Seq...>> {
       using S = tile_shape_t<Src>;
       using type = shape<(Dim != Seq
                           ? S::static_extent(Seq)
                           : ((S::static_extent(Dim) * bits_v<tile_element_t<Src>>) / bits_v<E>) )...>;
     };

     template<typename E, size_t Dim, typename Src>
     using repack_shape_t = typename repack_shape<E, Dim, Src, make_index_sequence<tile_rank_v<Src>>>::type;
     
     template<typename E, size_t Dim, typename Src>
     constexpr inline bool repack_constraints_v =
       (Dim < tile_rank_v<Src>) &&
       ((tile_shape_t<Src>::static_extent(Dim) * bits_v<tile_element_t<Src>>) % bits_v<E> == 0) &&
       (((tile_shape_t<Src>::static_extent(Dim) * bits_v<tile_element_t<Src>>) % 8 == 0) || is_same_v<tile_element_t<Src>, E>) &&
       tile_shape<repack_shape_t<E, Dim, Src>>;
   }

   template<scalar E, integral auto Dim, tile_like Src>
   requires (Dim >= 0) && detail::repack_constraints_v<detail::remove_cv_t<E>, size_t(Dim), Src>
   __tile_builtin__ tile<detail::remove_cv_t<E>, detail::repack_shape_t<detail::remove_cv_t<E>, size_t(Dim), Src>> repack(Src src, integral_constant<Dim> = {}) noexcept;

   template<integral_tile Lhs, integral_tile Rhs>
   requires arithmetic_tile_convertible<Lhs, Rhs>
   __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> mulhi(Lhs lhs, Rhs rhs) noexcept;

    namespace detail {      
      template<size_t, typename, typename> struct reduction_shape { };
      
      template<size_t D, typename S, size_t... Seq> struct reduction_shape<D, S, index_sequence<Seq...>> {
	using type = shape<(Seq == D ? 1 : S::static_extent(Seq))...>;
      };

      template<size_t D, typename S>
      using reduction_shape_t = typename reduction_shape<D, S, make_index_sequence<S::rank()>>::type;
    } // namespace detail

    template<tile_like Tile, size_t D> 
    requires (D < tile_rank_v<Tile>) 
    using reduction_result_t = tile<tile_element_t<Tile>, detail::reduction_shape_t<D, tile_shape_t<Tile>>>;

    /// Maximum Element Reduction
    template<integral auto Dimension, arithmetic_tile Tile> 
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      reduce_max(Tile a, integral_constant<Dimension> dim = {}) noexcept;

    template<integral auto Dimension, 
	     nan_propagation_mode Nan = default_nan_propagation_mode(), 
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(), 
             arithmetic_tile Tile> 
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
   	      && detail::compatible_nan_propagation_mode_v<Nan, Tile> 
	      && detail::compatible_subnormals_rounding_mode_v<SubMode, Tile> 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      reduce_max(Tile a, integral_constant<Dimension> dim, 
			    nan_propagation_mode_constant<Nan>, 
			    subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    /// Minimum Element Reduction
    template<integral auto Dimension, arithmetic_tile Tile> 
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
    reduce_min(Tile a, integral_constant<Dimension> dim = {}) noexcept;

    template<integral auto Dimension, 
	     nan_propagation_mode Nan = default_nan_propagation_mode(), 
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(), 
             arithmetic_tile Tile> 
    requires (Dimension >= 0)  && (Dimension < tile_rank_v<Tile>) 
	      && detail::compatible_nan_propagation_mode_v<Nan, Tile> 
	      && detail::compatible_subnormals_rounding_mode_v<SubMode, Tile> 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      reduce_min(Tile a, integral_constant<Dimension> dim, 
			    nan_propagation_mode_constant<Nan>, 
			    subnormals_rounding_mode_constant<SubMode> = {}) noexcept;
    
    /// max_element reduction
    template<integral auto Dimension, arithmetic_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ 
    reduction_result_t<tile_with_element_t<Tile, typename Tile::index_type>, Dimension>
	  max_element(Tile a, integral_constant<Dimension> dim = {}) noexcept;

    /// min_element reduction
    template<integral auto Dimension, arithmetic_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ 
    reduction_result_t<tile_with_element_t<Tile, typename Tile::index_type>, Dimension>
	  min_element(Tile a, integral_constant<Dimension> dim = {}) noexcept;
        
    /// Logical Conjunction Reduction
    template<integral auto Dimension, bool_tile_convertible Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ reduction_result_t<tile_with_element_t<Tile, bool>, Dimension>
    all_of(Tile a, integral_constant<Dimension> dim = {}) noexcept;

    /// Logical Disjunction Reduction
    template<integral auto Dimension, bool_tile_convertible Tile> 
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
    __tile_builtin__ reduction_result_t<tile_with_element_t<Tile, bool>, Dimension> 
    any_of(Tile a, integral_constant<Dimension> dim = {}) noexcept;
 
    template<integral auto Dimension, integral_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ reduction_result_t<Tile, Dimension>
    reduce_bitand(Tile a, integral_constant<Dimension> = {}) noexcept;

    template<integral auto Dimension, integral_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ reduction_result_t<Tile, Dimension>
    reduce_bitor(Tile a, integral_constant<Dimension> = {}) noexcept;

    template<integral auto Dimension, integral_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ reduction_result_t<Tile, Dimension>
    reduce_bitxor(Tile a, integral_constant<Dimension> = {}) noexcept;

    /// Summation Reduction
    template<integral auto Dimension, 
             arithmetic_tile Tile> 
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      sum(Tile a, integral_constant<Dimension> = {}) noexcept;

    template<integral auto Dimension, 
             rounding_mode Mode = default_rounding_mode(), 
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(), 
             arithmetic_tile Tile> 
    requires ((Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
              && detail::precise_rounding_mode_v<Mode>
              && detail::compatible_rounding_mode_v<Mode, Tile> 
              && detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>) 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      sum(Tile a, 
		  integral_constant<Dimension>, 
		  rounding_mode_constant<Mode>, 
		  subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    /// Product Reduction
    template<integral auto Dimension, arithmetic_tile Tile> 
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      prod(Tile a, integral_constant<Dimension> = {}) noexcept;

    template<integral auto Dimension, 
             rounding_mode Mode = default_rounding_mode(), 
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	     arithmetic_tile Tile>
    requires ((Dimension >= 0) && (Dimension < tile_rank_v<Tile>) 
              && detail::precise_rounding_mode_v<Mode>
              && detail::compatible_rounding_mode_v<Mode, Tile> 
              && detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>) 
    __tile_builtin__ reduction_result_t<Tile, Dimension> 
	      prod(Tile a, 
		   integral_constant<Dimension>, 
		   rounding_mode_constant<Mode>, 
		   subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

    enum class __tile_builtin__ scan_direction : unsigned char {
      forward = 0,
      backward = 1
    };

    namespace detail {
      template<scan_direction Direction>
      constexpr inline bool scan_direction_enumerator_v = (Direction <= scan_direction::backward);
    } // namespace detail
    
    template<scan_direction Value>
    requires detail::scan_direction_enumerator_v<Value>
    struct scan_direction_constant { 
      using type = scan_direction_constant;
      using value_type = scan_direction;
      static constexpr scan_direction value = Value;

      __tile__ __host__ __device__ constexpr operator scan_direction() const noexcept {
	return value;
      }
      __tile__ __host__ __device__ constexpr scan_direction operator()() const noexcept {
        return value;
      }
    };

    using scan_forward_t = scan_direction_constant<scan_direction::forward>;
    using scan_backward_t = scan_direction_constant<scan_direction::backward>;

    template<integral auto Dimension,
             scan_direction Direction = scan_direction::forward,
             arithmetic_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ Tile partial_sum(Tile a, integral_constant<Dimension> = {}, scan_direction_constant<Direction> = {}) noexcept;

    template<integral auto Dimension,
             rounding_mode Mode = default_rounding_mode(),
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
             scan_direction Direction = scan_direction::forward,
             arithmetic_tile Tile>
    requires ((Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
              && detail::precise_rounding_mode_v<Mode>
              && detail::compatible_rounding_mode_v<Mode, Tile>
              && detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>)
    __tile_builtin__ Tile partial_sum(Tile a,
                                      integral_constant<Dimension>,
                                      rounding_mode_constant<Mode>,
                                      subnormals_rounding_mode_constant<SubMode> = {},
                                      scan_direction_constant<Direction> = {}) noexcept;

    template<integral auto Dimension,
             scan_direction Direction = scan_direction::forward,
             arithmetic_tile Tile>
    requires (Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
    __tile_builtin__ Tile partial_prod(Tile a, integral_constant<Dimension> = {}, scan_direction_constant<Direction> = {}) noexcept;

    template<integral auto Dimension,
             rounding_mode Mode = default_rounding_mode(),
             subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
             scan_direction Direction = scan_direction::forward,
             arithmetic_tile Tile>
    requires ((Dimension >= 0) && (Dimension < tile_rank_v<Tile>)
              && detail::precise_rounding_mode_v<Mode>
              && detail::compatible_rounding_mode_v<Mode, Tile>
              && detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>)
    __tile_builtin__ Tile partial_prod(Tile a,
                                       integral_constant<Dimension>,
                                       rounding_mode_constant<Mode>,
                                       subnormals_rounding_mode_constant<SubMode> = {},
                                       scan_direction_constant<Direction> = {}) noexcept;

    template<tile_like T> constexpr inline size_t tile_size_v = shape_size_v<tile_shape_t<T>>;

    //iota
    template<integral_tile T>
    requires ((tile_size_v<T> - 1) <= detail::numeric_limits_max<tile_element_t<T>>())
    __tile_builtin__ T iota() noexcept; 

    template<loadable_tile Tile>
    __applicable_tile_hints__("latency")
    __tile_builtin__ tile_load_t<Tile> load(Tile ptrs) noexcept;

    template<loadable_tile Tile, 
             bool_tile_convertible Mask>
    requires broadcastable_to<Mask, tile_shape_t<Tile>>
    __applicable_tile_hints__("latency")
    __tile_builtin__ tile_load_t<Tile> load_masked(Tile ptrs, Mask mask) noexcept;

    template<loadable_tile Tile, 
             bool_tile_convertible Mask,
             tile_like Padding>
    requires broadcastable_to<Mask, tile_shape_t<Tile>>
          && broadcastable_to<Padding, tile_shape_t<Tile>>
          && non_narrowing_scalar_convertible_to<tile_element_t<Padding>,
						 tile_element_t<tile_load_t<Tile>>>
    __applicable_tile_hints__("latency")
    __tile_builtin__ tile_load_t<Tile> load_masked(Tile ptrs, Mask mask, Padding padding) noexcept;

    template<memory_order Order, 
             thread_scope Scope = default_thread_scope(),
             loadable_tile Tile>
    requires read_memory_order<Order>
    __applicable_tile_hints__("latency")
    __tile_builtin__ tile_load_t<Tile> atomic_load(Tile ptrs, 
						   memory_order_constant<Order> = {},
						   thread_scope_constant<Scope> = {}) noexcept;

    template<memory_order Order, 
             thread_scope Scope = default_thread_scope(),
             loadable_tile Tile,
             bool_tile_convertible Mask>
    requires broadcastable_to<Mask, tile_shape_t<Tile>>
          && read_memory_order<Order>
    __applicable_tile_hints__("latency")
    __tile_builtin__ tile_load_t<Tile> atomic_load_masked(Tile ptrs, 
							  Mask mask,
							  memory_order_constant<Order> = {}, 
							  thread_scope_constant<Scope> = {}) noexcept;

    template<memory_order Order, 
             thread_scope Scope = default_thread_scope(),
             loadable_tile Tile, 
             bool_tile_convertible Mask,
             tile_like Padding>
    requires broadcastable_to<Mask, tile_shape_t<Tile>>
          && broadcastable_to<Padding, tile_shape_t<Tile>>
          && non_narrowing_scalar_convertible_to<tile_element_t<Padding>,
						 tile_element_t<tile_load_t<Tile>>>
          && read_memory_order<Order>
    __applicable_tile_hints__("latency")
    __tile_builtin__ tile_load_t<Tile> atomic_load_masked(Tile ptrs, 
							  Mask mask,
							  Padding padding, 
							  memory_order_constant<Order> order = {}, 
							  thread_scope_constant<Scope> scope = {}) noexcept;

    template<storeable_tile Ptrs, 
             non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Value,
	     typename = tile_load_t<Ptrs>>
    __applicable_tile_hints__("latency")
    __tile_builtin__ void store(Ptrs ptrs, Value value) noexcept;

    template<storeable_tile Ptrs, 
             non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Value, 
             bool_tile_convertible Mask,
	     typename = tile_load_t<Ptrs>>
    requires broadcastable_to<Mask, tile_shape_t<Ptrs>>
    __applicable_tile_hints__("latency")
    __tile_builtin__ void store_masked(Ptrs ptrs, Value value, Mask mask) noexcept;

    template<memory_order Order, 
             thread_scope Scope = default_thread_scope(),
             storeable_tile Ptrs,
             non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Value,
	     typename = tile_load_t<Ptrs>>
    requires write_memory_order<Order>
    __applicable_tile_hints__("latency")
    __tile_builtin__ void atomic_store(Ptrs ptrs,
				       Value value,
				       memory_order_constant<Order> = {}, 
				       thread_scope_constant<Scope> = {}) noexcept;

    template<memory_order Order, 
             thread_scope Scope = default_thread_scope(),
             storeable_tile Ptrs, 
             non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Value, 
             bool_tile_convertible Mask,
	     typename = tile_load_t<Ptrs>>
    requires broadcastable_to<Mask, tile_shape_t<Ptrs>>
          && write_memory_order<Order>
    __applicable_tile_hints__("latency")
    __tile_builtin__ void atomic_store_masked(Ptrs ptrs, 
					      Value value, 
					      Mask mask,
					      memory_order_constant<Order> = {}, 
					      thread_scope_constant<Scope> = {}) noexcept;

    namespace detail {
      template<typename, typename, typename> struct tile_permutation { };
      template<typename E, typename S, typename M, size_t... Seq>
      struct tile_permutation<tile<E, S>, M, index_sequence<Seq...>> {
	using type = tile<E, shape<S::static_extent(M::mapping(Seq))...>>;
      };
      template<typename E, typename M, size_t... Seq>
      struct tile_permutation<E, M, index_sequence<Seq...>> {
	using type = E;
      };
    }

    template<tile_like Tile, typename Map>
    requires detail::is_dimension_map_v<detail::remove_cv_t<Map>> && (tile_rank_v<Tile> == Map::rank())
    using tile_permutation_t = typename detail::tile_permutation<detail::remove_cv_t<Tile>,
								 detail::remove_cv_t<Map>,
								 detail::make_index_sequence<Map::rank()>>::type;

    /// Permutation
    template<typename Map, tile_like Tile>
    requires detail::is_dimension_map_v<detail::remove_cv_t<Map>> && (Map::rank() == tile_rank_v<Tile>)
    __tile_builtin__ tile_permutation_t<Tile, Map> permute(Tile, Map = {}) noexcept;

    namespace detail {
      template<typename T> struct tile_transpose {
	using type = T;
      };
      template<typename E, size_t N, size_t M, size_t... Exts>
      struct tile_transpose<tile<E, shape<N, M, Exts...>>> {
	using type = tile<E, shape<M, N, Exts...>>;
      };
    }

    template<tile_like T>
    using tile_transpose_t = typename detail::tile_transpose<detail::remove_cv_t<T>>::type;

    /// Matrix Transpose
    template<tile_like Tile>
    __tile_builtin__ tile_transpose_t<Tile> transpose(Tile) noexcept;

    /// Reshape 
    template<tile_shape Shape, tile_like Tile>
    requires (tile_size_v<Tile> == shape_size_v<Shape>)
    __tile_builtin__ tile<tile_element_t<Tile>, detail::remove_cv_t<Shape>> reshape(Tile x, Shape = {}) noexcept;

    /// Broadcast
    template<tile_shape Shape, tile_like Tile>
    requires broadcastable_to<Tile, Shape> 
    __tile_builtin__ tile<tile_element_t<Tile>, Shape> broadcast(Tile x, Shape = {}) noexcept;

    /// Elementwise Conversion
    template<scalar Element, tile_like Source>
    requires tile_convertible_to<Source, tile_with_element_t<Source, Element>>
    __tile_builtin__ tile_with_element_t<Source, Element> element_cast(Source src) noexcept;

    template<scalar Element,
             tile_like Source,
             typename... /* placeholder to disallow explicit Mode parameter */,
             rounding_mode Mode>
    requires tile_convertible_to<Source, tile_with_element_t<Source, Element>>
    && detail::compatible_element_cast_rounding_mode_v<Mode, detail::tile_element_t<Source>, Element>
    __tile_builtin__ tile_with_element_t<Source, Element> element_cast(Source src, rounding_mode_constant<Mode> rounding_mode) noexcept;

    template<scalar Element, 
             basic_floating_point_tile Source,
             typename... /* placeholder; disallows explicitly specifying Saturation */,
             saturation_mode Saturation> 
        requires tile_convertible_to<Source, tile_with_element_t<Source, Element>> 
        && detail::is_integral_v<Element>
        && detail::saturation_mode_enumerator_v<Saturation>
    __tile_builtin__ tile_with_element_t<Source, Element> 
    element_cast(Source src, saturation_mode_constant<Saturation> saturation_mode) noexcept;  

    namespace detail {

    template<typename Lhs, typename Rhs>
    constexpr inline bool same_integral_bitwidth_v =
      integral_scalar<Lhs> && integral_scalar<Rhs> && sizeof(Lhs) == sizeof(Rhs);

    template<typename Ele, typename Acc>
    constexpr inline bool integer_mma_v =
      integral_scalar<Ele> && (sizeof(Ele) * TILE_CHAR_BIT == 8) &&
      integral_scalar<Acc> && is_signed_v<Acc> && (sizeof(Acc) * TILE_CHAR_BIT == 32);

    template<typename Ele, typename Acc>
    constexpr inline bool low_precision_mma_v =
      (is_same_v<Ele, __nv_fp8_e4m3> || is_same_v<Ele, __nv_fp8_e5m2> || is_float16_v<Ele>) &&
      (is_float16_v<Acc> || is_float32_v<Acc>);

    template<typename Ele, typename Acc>
    constexpr inline bool medium_precision_mma_v =
      (is_same_v<Ele, __nv_tf32> || is_bfloat16_v<Ele> || is_float32_v<Ele>) && is_float32_v<Acc>;

    template<typename Ele, typename Acc>
    constexpr inline bool high_precision_mma_v =
      is_float64_v<Ele> && is_float64_v<Acc>;
      
    template<typename Lhs, typename Rhs, typename Acc>
    constexpr inline bool mma_element_compatible_v =
      (same_integral_bitwidth_v<Lhs, Rhs> || same_underlying_floating_point_type_v<Lhs, Rhs>) &&
      (integer_mma_v<Lhs, Acc> ||
       low_precision_mma_v<Lhs, Acc> ||
       medium_precision_mma_v<Lhs, Acc> ||
       high_precision_mma_v<Lhs, Acc>);

    template<typename Lhs, typename Rhs, typename Acc>
    constexpr inline bool mma_rank2_compatible_v =
      (Lhs::static_extent(1) == Rhs::static_extent(0)) &&
      (Lhs::static_extent(0) == Acc::static_extent(0)) &&
      (Rhs::static_extent(1) == Acc::static_extent(1));

    template<typename Lhs, typename Rhs, typename Acc>
    constexpr inline bool mma_rank3_compatible_v =
      (Lhs::static_extent(2) == Rhs::static_extent(1)) &&
      (Lhs::static_extent(1) == Acc::static_extent(1)) &&
      (Rhs::static_extent(2) == Acc::static_extent(2)) &&
      (Lhs::static_extent(0) == 1 || Lhs::static_extent(0) == Acc::static_extent(0)) &&
      (Rhs::static_extent(0) == 1 || Rhs::static_extent(0) == Acc::static_extent(0));
      
    template<typename Lhs, typename Rhs, typename Acc>
    constexpr inline bool mma_compatible_v =
      mma_element_compatible_v<tile_element_t<Lhs>, tile_element_t<Rhs>, tile_element_t<Acc>> &&
      (tile_rank_v<Lhs> == tile_rank_v<Rhs>) &&
      (tile_rank_v<Rhs> == tile_rank_v<Acc>) &&
      ((tile_rank_v<Acc> == 2 && mma_rank2_compatible_v<tile_shape_t<Lhs>, tile_shape_t<Rhs>, tile_shape_t<Acc>>) ||
       (tile_rank_v<Acc> == 3 && mma_rank3_compatible_v<tile_shape_t<Lhs>, tile_shape_t<Rhs>, tile_shape_t<Acc>>));
    } // namespace detail
  
    template<typename Lhs, typename Rhs, typename Acc> 
    concept mma_compatible = numeric_tile<Lhs> && numeric_tile<Rhs> && numeric_tile<Acc> &&
			     detail::mma_compatible_v<detail::remove_cv_t<Lhs>,
						      detail::remove_cv_t<Rhs>,
						      detail::remove_cv_t<Acc>>;

    /// Batched Matrix Multiply Accumulate (MMA)
    template<numeric_tile Lhs, numeric_tile Rhs, numeric_tile Acc>
    requires mma_compatible<Lhs, Rhs, Acc>
    __tile_builtin__ Acc mma(Lhs, Rhs, Acc) noexcept;

    namespace detail {
      template<typename Lhs, typename Rhs>
      constexpr inline bool matmul_rank2_compatible_v =
	(Lhs::static_extent(1) == Rhs::static_extent(0)) &&
	tile_shape<shape<Lhs::static_extent(0), Rhs::static_extent(1)>>;

      template<typename Lhs, typename Rhs>
      constexpr inline bool matmul_rank3_compatible_v =
	(Lhs::static_extent(2) == Rhs::static_extent(1)) &&
	(Lhs::static_extent(0) == 1 || Rhs::static_extent(0) == 1 || Lhs::static_extent(0) == Rhs::static_extent(0)) &&
	tile_shape<shape<max(Lhs::static_extent(0), Rhs::static_extent(0)), Lhs::static_extent(1), Rhs::static_extent(2)>>;

      template<typename Lhs, typename Rhs>
      constexpr inline bool matmul_element_compatible_v =
	(integral_scalar<Lhs> && integral_scalar<Rhs> && 
	 (sizeof(Lhs) * TILE_CHAR_BIT == 8) && (sizeof(Rhs) * TILE_CHAR_BIT == 8)) ||
	same_underlying_floating_point_type_v<Lhs, Rhs>;
      
      template<typename Lhs, typename Rhs>
      constexpr inline bool matmul_compatible_v =
	matmul_element_compatible_v<tile_element_t<Lhs>, tile_element_t<Rhs>> &&
        (tile_rank_v<Lhs> == tile_rank_v<Rhs>) &&
	((tile_rank_v<Lhs> == 2 && matmul_rank2_compatible_v<tile_shape_t<Lhs>, tile_shape_t<Rhs>>) ||
	 (tile_rank_v<Lhs> == 3 && matmul_rank3_compatible_v<tile_shape_t<Lhs>, tile_shape_t<Rhs>>));
    } // namespace detail

    template<typename Lhs, typename Rhs> concept matmul_compatible =
      numeric_tile<Lhs> && numeric_tile<Rhs> && detail::matmul_compatible_v<detail::remove_cv_t<Lhs>,
									    detail::remove_cv_t<Rhs>>;
    namespace detail {
      template<typename T> struct matmul_element_result { };
      
      template<integral_scalar T> struct matmul_element_result<T> { using type = int32_t; };
      
      template<typename T>
      requires (same_as<T, __nv_fp8_e4m3> || same_as<T, __nv_fp8_e5m2> || same_as<T, __half>)
      struct matmul_element_result<T> { using type = __half; };

      template<typename T>
      requires (same_as<T, __nv_tf32> || same_as<T, __nv_bfloat16> || same_as<T, float>)
      struct matmul_element_result<T> { using type = float; };

      template<same_as<double> T>
      struct matmul_element_result<T> { using type = double; };

      template<typename Lhs, typename Rhs> struct matmul_shape_result { };

      template<typename Lhs, typename Rhs>
      requires (Lhs::rank() == 2)
      struct matmul_shape_result<Lhs, Rhs> {
	using type = shape<Lhs::static_extent(0), Rhs::static_extent(1)>;
      };

      template<typename Lhs, typename Rhs>
      requires (Lhs::rank() == 3)
      struct matmul_shape_result<Lhs, Rhs> {
	      static constexpr size_t dummy = max(Lhs::static_extent(0), Rhs::static_extent(0));
	using type = shape<dummy, Lhs::static_extent(1), Rhs::static_extent(2)>;
      };
    } // namespace detail

    template <numeric_tile Lhs, numeric_tile Rhs>
    requires matmul_compatible<Lhs, Rhs>
    using matmul_result_t = tile<typename detail::matmul_element_result<tile_element_t<Lhs>>::type,
				 typename detail::matmul_shape_result<tile_shape_t<Lhs>, tile_shape_t<Rhs>>::type>;

    /// Batched Matrix Multiply
    template<numeric_tile Lhs, numeric_tile Rhs> 
    requires matmul_compatible<Lhs, Rhs> 
    __tile_builtin__ matmul_result_t<Lhs, Rhs> matmul(Lhs, Rhs) noexcept;


   namespace detail {
     template<typename Lhs, typename LhsScale, typename Rhs, typename RhsScale> constexpr inline bool mma_scaled_rank2_compatible_v =
       (Lhs::static_extent(0) == LhsScale::static_extent(0)) &&
       (Rhs::static_extent(1) == RhsScale::static_extent(1)) &&
       (LhsScale::static_extent(1) == RhsScale::static_extent(0)) &&
       (Lhs::static_extent(1) % LhsScale::static_extent(1) == 0);

     template<typename Lhs, typename LhsScale, typename Rhs, typename RhsScale, typename Acc> constexpr inline bool mma_scaled_rank3_compatible_v =
       (Lhs::static_extent(1) == LhsScale::static_extent(1)) &&
       (Rhs::static_extent(2) == RhsScale::static_extent(2)) &&
       (LhsScale::static_extent(2) == RhsScale::static_extent(1)) &&
       (Lhs::static_extent(2) % LhsScale::static_extent(2) == 0) &&
       ((LhsScale::static_extent(0) == 1) || (LhsScale::static_extent(0) == Acc::static_extent(0))) &&
       ((RhsScale::static_extent(0) == 1) || (RhsScale::static_extent(0) == Acc::static_extent(0)));

     template<typename Lhs, typename LhsScale, typename Rhs, typename RhsScale, typename Acc, size_t Vec>
     constexpr inline bool mma_scaled_element_constraints_v =
       is_same_v<Lhs, Rhs> &&
       is_same_v<LhsScale, RhsScale> &&
       is_float32_v<Acc> &&
       ((is_same_v<Lhs, __nv_fp8_e4m3> && is_same_v<LhsScale, __nv_fp8_e8m0> && (Vec == 32)) ||
        (is_same_v<Lhs, __nv_fp8_e5m2> && is_same_v<LhsScale, __nv_fp8_e8m0> && (Vec == 32)) ||
        (is_same_v<Lhs, __nv_fp4_e2m1> && is_same_v<LhsScale, __nv_fp8_e8m0> && ((Vec == 16) || (Vec == 32))) ||
        (is_same_v<Lhs, __nv_fp4_e2m1> && is_same_v<LhsScale, __nv_fp8_e4m3> && (Vec == 16)) ||
        (is_same_v<Lhs, __nv_fp4_e2m1> && is_same_v<LhsScale, __nv_fp8_ue5m3> && ((Vec == 16) || (Vec == 32))));

     template<typename Lhs, typename LhsScale>
     constexpr inline size_t mma_scaled_vec_size_v =
       (Lhs::rank() == 2) ? (Lhs::static_extent(1) / LhsScale::static_extent(1)) : (Lhs::static_extent(2) / LhsScale::static_extent(2));
     
      template<typename Lhs, typename LhsScale, typename Rhs, typename RhsScale, typename Acc>
      constexpr inline bool scaled_mma_constraints_v =
        (tile_rank_v<Lhs> == tile_rank_v<LhsScale>) &&
        (tile_rank_v<Lhs> == tile_rank_v<Rhs>) &&
        (tile_rank_v<Lhs> == tile_rank_v<RhsScale>) &&
        (tile_rank_v<Lhs> == tile_rank_v<Acc>) &&
        ((tile_rank_v<Lhs> == 2) || (tile_rank_v<Lhs> == 3)) &&
        mma_scaled_element_constraints_v<tile_element_t<Lhs>,
                                         tile_element_t<LhsScale>,
                                         tile_element_t<Rhs>,
                                         tile_element_t<RhsScale>,
                                         tile_element_t<Acc>,
                                         mma_scaled_vec_size_v<tile_shape_t<Lhs>, tile_shape_t<LhsScale>>> &&
        ((tile_rank_v<Lhs> == 2 &&
          mma_rank2_compatible_v<tile_shape_t<Lhs>, tile_shape_t<Rhs>, tile_shape_t<Acc>> &&
          mma_scaled_rank2_compatible_v<tile_shape_t<Lhs>, tile_shape_t<LhsScale>, tile_shape_t<Rhs>, tile_shape_t<RhsScale>>) ||
         (tile_rank_v<Lhs> == 3 &&
          mma_rank3_compatible_v<tile_shape_t<Lhs>, tile_shape_t<Rhs>, tile_shape_t<Acc>> &&
          mma_scaled_rank3_compatible_v<tile_shape_t<Lhs>, tile_shape_t<LhsScale>, tile_shape_t<Rhs>, tile_shape_t<RhsScale>, tile_shape_t<Acc>>));
    }

    template<numeric_tile Lhs, numeric_tile LhsScale, 
             numeric_tile Rhs, numeric_tile RhsScale,
             numeric_tile Acc>
    requires detail::scaled_mma_constraints_v<Lhs, LhsScale, Rhs, RhsScale, Acc>
    __tile_builtin__ Acc scaled_mma(Lhs lhs, LhsScale lhsScale, 
                                    Rhs rhs, RhsScale rhsScale, 
                                    Acc acc) noexcept;

   namespace detail {
     template<typename T, typename U, size_t D, typename>
     struct concatenation_shape { };

     template<typename T, typename U, size_t D, size_t... Seq>
     struct concatenation_shape<T, U, D, index_sequence<Seq...>> {
       using S1 = tile_shape_t<T>;
       using S2 = tile_shape_t<U>;
       using type = shape<(Seq == D ? S1::static_extent(Seq) + S2::static_extent(Seq) : S1::static_extent(Seq))...>;
     };
  
     template<typename T, typename U, size_t D>
     using concatenation_shape_t = typename concatenation_shape<T, U, D, make_index_sequence<tile_rank_v<T>>>::type;
  
     template<typename T, typename U, size_t D, typename> constexpr inline bool concatenation_compatible_v = false;
     template<typename T, typename U, size_t D, size_t... Seq>
     constexpr inline bool concatenation_compatible_v<T, U, D, index_sequence<Seq...>> =
       is_same_v<T, U> && D < tile_rank_v<T> && tile_shape<concatenation_shape_t<T, U, D>>;
   } // namespace detail

   template<typename T, typename U, size_t D>
   concept concatenation_compatible = tile_like<T> && tile_like<U> &&
				      detail::concatenation_compatible_v<detail::remove_cv_t<T>, detail::remove_cv_t<U>, D, detail::make_index_sequence<tile_rank_v<T>>>;


   template<tile_like T, tile_like U, size_t D>
   requires concatenation_compatible<T, U, D>
   using concatenation_t = tile<tile_element_t<T>, detail::concatenation_shape_t<T, U, D>>;

    /// Concatenation
    template<integral auto Dim, tile_like Lhs, tile_like Rhs>
    requires (Dim >= 0) && concatenation_compatible<Lhs, Rhs, Dim> 
    __tile_builtin__ concatenation_t<Lhs, Rhs, Dim>
    cat(Lhs l, Rhs r, integral_constant<Dim> = {}) noexcept;

    /// Elementwise Conditional Operator
    template<tile_like T, bool_tile_convertible C>
    requires broadcastable_to<C, tile_shape_t<T>>
    __tile_builtin__ T select(C condition, T lhs, T rhs) noexcept;

    namespace detail {
      template<typename, typename, typename>
      constexpr inline bool extractable_from_v = false;
      
      template<typename S, typename T, size_t... Seq>
      constexpr inline bool extractable_from_v<S, T, index_sequence<Seq...>> =
	S::rank() == tile_rank_v<T> &&
	((tile_shape_t<T>::static_extent(Seq) % S::static_extent(Seq) == 0) && ...);
    } // namespace detail

    template<typename S, typename T>
    concept extractable_from = tile_shape<S> && tile_like<T> &&
			       detail::extractable_from_v<detail::remove_cv_t<S>,
							  detail::remove_cv_t<T>,
							  detail::make_index_sequence<S::rank()>>;

    namespace detail {
      template<typename Shape, typename Tile, typename... Indices>
      __tile_builtin__ tile<tile_element_t<Tile>, Shape> extract(Tile, Shape, Indices...);

      template<typename Subtile, typename Tile, typename... Indices>
      __tile_builtin__ Tile insert(Tile, Subtile, Indices...);
    }

    /// Subtile Extraction
    template<tile_shape Shape, tile_like Tile, typename... Indices>
    requires extractable_from<Shape, Tile>
      && (sizeof...(Indices) == Shape::rank())
      && (detail::is_convertible_v<Indices, size_t> && ...)
    __tile__ tile<tile_element_t<Tile>, Shape> extract(Tile x, Shape s, Indices... idx) noexcept {
      return detail::extract(x, s, size_t(idx)...);
    }


  /// Subtile Insertion
  template<tile_like Subtile, tile_like Tile, typename... Indices>
  requires extractable_from<tile_shape_t<Subtile>, Tile>
    && (sizeof...(Indices) == tile_rank_v<Subtile>)
    && (detail::is_convertible_v<Indices, size_t> && ...)
    && non_narrowing_scalar_convertible_to<detail::tile_element_t<Subtile>,
                                           detail::tile_element_t<Tile>>
  [[nodiscard]] __tile__ Tile insert(Tile dst, Subtile src, Indices... idx) noexcept {
    return detail::insert(dst, element_cast<detail::tile_element_t<Tile>>(src),
                          size_t(idx)...);
  }

  namespace experimental {

#ifdef __CUDACC_TILE_PREVIEW_SIMT__

    /// string literal wrapper
    template <size_t N>
    struct __tile_builtin__ fixedString {
      char data[N]{};
      consteval __host__ __device__ __tile__  fixedString(const char (&str)[N]) {
          for (size_t i = 0; i < N; ++i)
              data[i] = str[i];
      }
    };

    //multiple output wrapper for SIMT interop
    template <typename... T>
    struct __tile_builtin__  multi_outputs {
      static_assert(sizeof...(T) > 0 && sizeof...(T) < 5, "only up to 5 outputs are supported!");
    }; 

    template <typename T0>
    struct __tile_builtin__ multi_outputs<T0> { T0 F0; };

    template <typename T0, typename T1>
    struct __tile_builtin__ multi_outputs<T0, T1> { T0 F0; T1 F1; };

    template <typename T0, typename T1, typename T2>
    struct __tile_builtin__ multi_outputs<T0, T1, T2> { T0 F0; T1 F1; T2 F2; };

    template <typename T0, typename T1, typename T2, typename T3>
    struct __tile_builtin__ multi_outputs<T0, T1, T2, T3> { T0 F0; T1 F1; T2 F2; T3 F3; };

    template <typename T0, typename T1, typename T2, typename T3, typename T4>
    struct __tile_builtin__ multi_outputs<T0, T1, T2, T3, T4> { T0 F0; T1 F1; T2 F2; T3 F3; T4 F4; };

#endif  /* __CUDACC_TILE_PREVIEW_SIMT__ */
  }  // namespace experimental

  /// Ceiling
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ ceil(Tile) noexcept;

  /// Floor
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ floor(Tile) noexcept;

  /// Hyperbolic cosine
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ cosh(Tile) noexcept;

  /// Cosine
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ cos(Tile) noexcept;

  namespace detail {
  // Only up to (and including) 32-bit wide signed integers (and booleans) are supported.
  template<typename T>
  constexpr inline bool is_compatible_integral_exponent_type_v =
      (integral_scalar_v<T> && is_signed_v<T> && sizeof(T) * TILE_CHAR_BIT <= 32)
      || is_same_v<T, bool>;
  } // namespace detail

  // Exponentiation
  template<basic_floating_point_tile Base, arithmetic_tile Exponent>
  requires arithmetic_tile_convertible<Base, Exponent>
  __tile_builtin__ arithmetic_tile_conversion_t<Base, Exponent>
   pow(Base base, Exponent exponent) noexcept;

  template<basic_floating_point_tile Base, integral_tile Exponent> 
    requires broadcast_compatible<Base, Exponent> 
    && detail::is_compatible_integral_exponent_type_v<tile_element_t<Exponent>>
  __tile_builtin__ mutual_broadcast_t<Base, Exponent, tile_element_t<Base>>
  powi(Base base, Exponent exponent) noexcept;

  /// Base Two Exponentiation
  template <subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	    basic_floating_point_tile Tile>
  requires detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>
  Tile __tile_builtin__ exp2(Tile, subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

  /// Base e Exponentiation
  template<rounding_mode Mode = rounding_mode::round_full,
	   basic_floating_point_tile Tile>
  requires detail::compatible_exp_tanh_rounding_mode_v<Mode, Tile>
  __tile_builtin__ Tile exp(Tile x,
			    rounding_mode_constant<Mode> = {}) noexcept;

  /// Base 2 Logarithm
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ log2(Tile) noexcept;

  /// Natural Logarithm
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ log(Tile) noexcept;

  /// Element-wise reciprocal square root
  template<subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	   basic_floating_point_tile Tile>
  requires detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>
  __tile_builtin__ Tile rsqrt(Tile, subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

  /// Hyperbolic sine
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ sinh(Tile) noexcept;

  /// Sine
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ sin(Tile) noexcept;
    
  /// Square Root
  template<rounding_mode Mode = default_rounding_mode(),
	   subnormals_rounding_mode SubMode = default_subnormals_rounding_mode(),
	   basic_floating_point_tile Tile>
  requires detail::compatible_rounding_mode_v<Mode, Tile>
	&& detail::compatible_subnormals_rounding_mode_v<SubMode, Tile>
        && (detail::precise_rounding_mode_v<Mode> || Mode == rounding_mode::round_approximate)
  __tile_builtin__ Tile sqrt(Tile x,
			     rounding_mode_constant<Mode> = {},
			     subnormals_rounding_mode_constant<SubMode> = {}) noexcept;

  /// Hyperbolic tangent
  template<rounding_mode Mode = rounding_mode::round_full,
	   basic_floating_point_tile Tile>
  requires detail::compatible_exp_tanh_rounding_mode_v<Mode, Tile>
  __tile_builtin__ Tile tanh(Tile x,
			     rounding_mode_constant<Mode> = {}) noexcept;

  /// Tangent
  template <basic_floating_point_tile Tile>
  Tile __tile_builtin__ tan(Tile) noexcept;

  template<arithmetic_tile Lhs, arithmetic_tile Rhs>
  requires arithmetic_tile_convertible<Lhs, Rhs>
        && basic_floating_point_tile<arithmetic_tile_conversion_t<Lhs, Rhs>>
  __tile_builtin__ arithmetic_tile_conversion_t<Lhs, Rhs> atan2(Lhs y, Rhs x) noexcept;

  template<typename T>
  struct layout_mapping_static_stride {
    __tile__ __host__ __device__ constexpr size_t operator()(typename T::rank_type idx) const noexcept
      requires requires { { T::static_stride(idx) } -> same_as<size_t>; } {
      return T::static_stride(idx);
    }
  };

  namespace detail {
    template<typename T> constexpr inline bool is_layout_mapping_v =
      is_copyable_v<T>
      && requires {
          requires !is_cv_qualified_v<typename T::extents_type>;
          requires extents_like<typename T::extents_type>;
          requires same_as<typename T::rank_type, typename T::extents_type::rank_type>;
          requires same_as<typename T::index_type, typename T::extents_type::index_type>;
          requires same_as<typename T::layout_type::template mapping<typename T::extents_type>, T>;
          { T::is_always_strided() } -> same_as<bool>;
          requires constant_evaluate_v<T::is_always_strided()>;
          { layout_mapping_static_stride<T>{}(declval<typename T::rank_type>()) } -> same_as<size_t>;
          { declval<T const&>().stride(declval<typename T::rank_type const&>()) } -> same_as<typename T::index_type>;
          { declval<T const&>().extents() } -> same_as<typename T::extents_type const&>;
      };
  } // namespace detail

  template<typename T> concept layout_mapping = detail::is_layout_mapping_v<detail::remove_cv_t<T>>;

  struct layout_right;
  
  template<extents_like Extents> struct _CUTILE_DECLSPEC_EMPTY_BASES layout_right_mapping : private Extents {
    static_assert(!detail::is_cv_qualified_v<Extents>, "In cuda::tiles::layout_right_mapping<E>, E may not be cv-qualified.");
    
    using extents_type = Extents;
    using rank_type = typename extents_type::rank_type;
    using index_type = typename extents_type::index_type;
    using layout_type = layout_right;

    __tile__ __host__ __device__ explicit layout_right_mapping(extents_type const& other) noexcept : extents_type{other} { }

    __tile__ __host__ __device__ static constexpr bool is_always_strided() noexcept { return true; }
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type dim) noexcept {
      if (extents_type::rank() == 1) return 1;
    
      size_t result{1};

      for(rank_type idx(extents_type::rank() - 1); idx != rank_type{0}; --idx) {
        if (dim == idx) break;
        
        if (extents_type::static_extent(idx) == dynamic_extent) {
          result = dynamic_extent;
        } else if (result == dynamic_extent) {
          result = dynamic_extent;
        } else {
          result *= extents_type::static_extent(idx);
        }
      }

      return result;
  }

  private:
    // If idx is in the range [Seq, rank()), yields product of extents at indices [idx + 1, rank() ).
    // Otherwise yields the product of the extents at indices [Seq + 1, rank() )
    template<size_t Seq>
    __tile__ __host__ __device__ index_type stride_helper(rank_type idx) const noexcept {
      if constexpr(Seq + 1 >= extents_type::rank()) {
        return 1;
      } else {
        index_type acc = stride_helper<Seq + 1>(idx);

        index_type stride{0};
        
        if constexpr (extents_type::static_extent(Seq + 1) == dynamic_extent) {
          stride = extents().extent(Seq + 1);
        } else {
          stride = extents_type::static_extent(Seq + 1);
        }

        bool mask = (idx <= Seq);
        
        return mask * acc * stride + !mask * acc;
      }
    }

  public:
    __tile__ __host__ __device__ index_type stride(rank_type idx) const noexcept {
      return stride_helper<0>(idx);
    }

    __tile__ __host__ __device__ extents_type const& extents() const noexcept {
      return static_cast<extents_type const&>(*this);
    }
  };

  struct layout_right {
    template<extents_like E> using mapping = layout_right_mapping<detail::remove_cv_t<E>>;
  };

  struct layout_left;
  
  template<extents_like Extents> struct _CUTILE_DECLSPEC_EMPTY_BASES layout_left_mapping : private Extents {
    static_assert(!detail::is_cv_qualified_v<Extents>, "In cuda::tiles::layout_left_mapping<E>, E may not be cv-qualified.");
    
    using extents_type = Extents;
    using rank_type = typename extents_type::rank_type;
    using index_type = typename extents_type::index_type;
    using layout_type = layout_left;

    __tile__ __host__ __device__ explicit layout_left_mapping(extents_type const& other) noexcept : extents_type{other} { }

    __tile__ __host__ __device__ static constexpr bool is_always_strided() noexcept { return true; }
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type dim) noexcept {
      if (extents_type::rank() == 1) return 1;
    
      size_t result{1};

      for(rank_type idx = 0; idx != extents_type::rank() - 1; ++idx) {
        if (dim == idx) break;
        
        if (extents_type::static_extent(idx) == dynamic_extent) {
          result = dynamic_extent;
        } else if (result == dynamic_extent) {
          result = dynamic_extent;
        } else {
          result *= extents_type::static_extent(idx);
        }
      }

      return result;
    }

  private:
    // If idx is in the range [0, Seq), yields the product of extents [0, idx - 1).
    // Otherwise, yields the product of the extents [0, Seq - 1)
    template<size_t Seq>
    __tile__ __host__ __device__ index_type stride_helper(rank_type idx) const noexcept {
      if constexpr(Seq <= 1) {
        return 1;
      } else {
        index_type acc = stride_helper<Seq - 1>(idx);

        index_type stride{0};
        if constexpr (extents_type::static_extent(Seq - 2) == dynamic_extent) {
          stride = extents().extent(Seq - 2);
        } else {
          stride = extents_type::static_extent(Seq - 2);
        }

        bool mask = (idx >= (Seq - 1));
        
        return mask * acc * stride + !mask * acc;
      }
    }
    
  public:
    __tile__ __host__ __device__ index_type stride(rank_type idx) const noexcept {
      return stride_helper<extents_type::rank()>(idx);
    }

    __tile__ __host__ __device__ extents_type const& extents() const noexcept {
      return static_cast<extents_type const&>(*this);
    }
  };

  struct layout_left {
    template<extents_like E> using mapping = layout_left_mapping<detail::remove_cv_t<E>>;
  };

  namespace detail {
    template<typename T, size_t> struct _CUTILE_DECLSPEC_EMPTY_BASES type_with_tag : public T {
      constexpr __tile__ __host__ __device__ type_with_tag(T obj) noexcept : T{static_cast<T&&>(obj)} { }
    };
  }

  template<typename T> requires (extents_like<T> || strides_like<T>) struct layout_strided;

namespace detail {

  template<typename T> struct extents_or_strides_interface {
    using stride_type = typename T::index_type;
    using rank_type = typename T::rank_type;
    
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type idx) noexcept {
      return T::static_extent(idx);
    }

    __tile__ __host__ __device__ static constexpr stride_type stride(T const& obj, rank_type idx) noexcept {
      return obj.extent(idx);
    }
  };
  
  template<strides_like T> struct extents_or_strides_interface<T> {
    using stride_type = typename T::stride_type;
    using rank_type = typename T::rank_type;
    
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type idx) noexcept {
      return T::static_stride(idx);
    }

    __tile__ __host__ __device__ static constexpr stride_type stride(T const& obj, rank_type idx) noexcept {
      return obj.stride(idx);
    }
  };
  
  
  template<typename E, typename S> constexpr inline bool layout_strided_constraints_v =
    same_as<typename E::index_type, typename extents_or_strides_interface<S>::stride_type> && (E::rank() == S::rank());
}

  template<extents_like Extents, typename Strides>
  requires ((extents_like<Strides> || strides_like<Strides>) && detail::layout_strided_constraints_v<Extents, Strides>)
  struct _CUTILE_DECLSPEC_EMPTY_BASES layout_strided_mapping : private detail::type_with_tag<Extents, 0>, detail::type_with_tag<Strides, 1> {
  private:
    using extents_base = detail::type_with_tag<Extents, 0>;
    using strides_base = detail::type_with_tag<Strides, 1>;
    
    static_assert(!detail::is_cv_qualified_v<Extents> && !detail::is_cv_qualified_v<Strides>,
                  "In cuda::tiles::layout_strided_mapping<E, S>, E and S may not be cv-qualified.");

  public:
    using extents_type = Extents;
    using rank_type = typename extents_type::rank_type;
    using index_type = typename extents_type::index_type;
    using layout_type = layout_strided<Strides>;

    __tile__ __host__ __device__ layout_strided_mapping(extents_type const& extents, Strides const& strides) noexcept
      : extents_base{extents}, strides_base{strides} { }

    __tile__ __host__ __device__ static constexpr bool is_always_strided() noexcept { return true; }
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type dim) noexcept {
      return detail::extents_or_strides_interface<Strides>::static_stride(dim);
    }

    __tile__ __host__ __device__ index_type stride(rank_type dim) const noexcept {
      return detail::extents_or_strides_interface<Strides>::stride(static_cast<strides_base const&>(*this), dim);
    }

    __tile__ __host__ __device__ extents_type const& extents() const noexcept {
      return static_cast<extents_type const&>(static_cast<extents_base const&>(*this));
    }
  };

  template<layout_mapping Lhs, layout_mapping Rhs>
  __tile__ __host__ __device__ constexpr bool layout_mapping_equal(Lhs const& lhs, Rhs const& rhs) noexcept {
    if constexpr(Lhs::extents_type::rank() == Rhs::extents_type::rank()) {
      return extents_equal(lhs.extents(), rhs.extents()) &&
             [&]<size_t... Seq>(detail::index_sequence<Seq...>){
               return ((lhs.stride(Seq) == rhs.stride(Seq)) && ...);
             }(detail::make_index_sequence<Lhs::extents_type::rank()>{});
    } else {
      return false;
    }
  }

  template<layout_mapping Lhs, layout_mapping Rhs>
  __tile__ __host__ __device__ constexpr bool operator==(Lhs const& lhs, Rhs const& rhs) noexcept {
    return layout_mapping_equal(lhs, rhs);
  }

   template<typename Strides>
   requires (extents_like<Strides> || strides_like<Strides>)
   struct layout_strided {
     static_assert(!detail::is_cv_qualified_v<Strides>, "In cuda::tiles::layout_strided<S>, S must not be cv-qualified.");
     
     template<extents_like E> using mapping = layout_strided_mapping<detail::remove_cv_t<E>, Strides>;
   };

  namespace detail {
    template<typename T, size_t V> struct integer_or_constant {
      __tile__ __host__ __device__ constexpr explicit integer_or_constant(T) noexcept { }
      __tile__ __host__ __device__ constexpr T get() const noexcept { return T(V); }
    };
    template<typename T> struct integer_or_constant<T, dynamic_extent> {
      __tile__ __host__ __device__  constexpr explicit integer_or_constant(T v) noexcept : value{v} { }
      __tile__ __host__ __device__ constexpr T get() const noexcept { return value; }
      
    private:
      T value;
    };

    template<typename T>
    __tile__ __host__ __device__ constexpr T least_multiple_bounded_below(T divisor, T bound) noexcept {
      return ((bound - 1) / divisor + 1) * divisor;
    }

    template<typename Extents, size_t Dim, size_t Stride>
    __tile__ __host__ __device__ constexpr auto padded_layout_actual_static_stride() noexcept {
      if constexpr(Dim >= Extents::rank()) {
        return Stride;
      } else if constexpr(Extents::static_extent(Dim) == dynamic_extent || Stride == dynamic_extent) {
        return dynamic_extent;
      } else {
        return least_multiple_bounded_below(Stride, Extents::static_extent(Dim));
      }
    }

    template<typename Extents, size_t Dim>
    __tile__ __host__ __device__ constexpr auto padded_layout_actual_stride(Extents const& extents, typename Extents::index_type stride) noexcept {
      if constexpr(Dim >= Extents::rank()) {
        return stride;
      } else {
        return least_multiple_bounded_below(stride, extents.extent(Dim));
      }
    }

    template<typename Extents, size_t Dim, size_t Stride>
    struct _CUTILE_DECLSPEC_EMPTY_BASES padded_extents : private Extents, detail::integer_or_constant<typename Extents::index_type, Stride> {
    private:
      using extents_base = Extents;
      using stride_base = detail::integer_or_constant<typename Extents::index_type, Stride>;

    public:
      using index_type = typename Extents::index_type;
      using rank_type = typename Extents::rank_type;

      __tile__ __host__ __device__ constexpr padded_extents(Extents const& extents, index_type stride) noexcept
        : extents_base{extents}, stride_base{stride} {
      }

    __tile__ __host__ __device__ static constexpr size_t rank() noexcept {
      return extents_base::rank();
    }

    __tile__ __host__ __device__ static constexpr size_t rank_dynamic() noexcept {
      return []<size_t... Seq>(detail::index_sequence<Seq...>){
        size_t rank{0};
        ((rank += (static_extent(Seq) == dynamic_extent)), ...);
        return rank;
      }(detail::make_index_sequence<rank()>{});
    }
                                                                           
      __tile__ __host__ __device__ static constexpr size_t static_extent(rank_type idx) noexcept {
        if (idx == Dim) {
          return padded_layout_actual_static_stride<Extents, Dim, Stride>();
        } else {
          return extents_base::static_extent(idx);
        }
      }

      __tile__ __host__ __device__ constexpr index_type extent(rank_type idx) const noexcept {
        index_type actual_stride = padded_layout_actual_stride<Extents, Dim>(underlying_extents(), this->stride_base::get());
        return (idx == Dim) * actual_stride + (idx != Dim) * this->extents_base::extent(idx);
      }

      __tile__ __host__ __device__ constexpr extents_base const& underlying_extents() const noexcept {
        return static_cast<extents_base const&>(*this);
      }
    };
  }

  namespace detail {
    template<typename T> __tile__ __host__ __device__ constexpr auto rank_of() { return T::rank(); }
  } // namespace detail

  template<size_t> struct layout_right_padded;
  
  template<extents_like Extents, size_t Stride>
  struct _CUTILE_DECLSPEC_EMPTY_BASES layout_right_padded_mapping : private detail::padded_extents<Extents, detail::rank_of<Extents>() - 1, Stride> {
  private:
    using base = detail::padded_extents<Extents, detail::rank_of<Extents>() - 1, Stride>;
    
  public:
    static_assert(!detail::is_cv_qualified_v<Extents>, "In cuda::tiles::layout_right_padded_mapping<E, S>, E may not be cv-qualified.");

    using extents_type = Extents;
    using rank_type = typename extents_type::rank_type;
    using index_type = typename extents_type::index_type;
    using layout_type = layout_right_padded<Stride>;

    template<typename T>
    requires detail::is_convertible_v<T, index_type>
    __tile__ __host__ __device__ explicit layout_right_padded_mapping(extents_type const& extents, T stride) noexcept
      : base{extents, index_type(stride)} {
    }

    __tile__ __host__ __device__ static constexpr bool is_always_strided() noexcept { return true; }
    
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type dim) noexcept {
      return layout_right_mapping<base>::static_stride(dim);
    }

    __tile__ __host__ __device__ index_type stride(rank_type dim) const noexcept {
      return layout_right_mapping<base>{*this}.stride(dim);
    }

    __tile__ __host__ __device__ extents_type const& extents() const noexcept {
      return this->base::underlying_extents();
    }
  };

  template<extents_like Extents, typename Stride> layout_right_padded_mapping(Extents const&, Stride)
     -> layout_right_padded_mapping<Extents, detail::extent_value_or_dynamic<detail::remove_cv_t<Stride>>>;

  template<size_t Stride> struct layout_right_padded {
    template<extents_like E> using mapping = layout_right_padded_mapping<detail::remove_cv_t<E>, Stride>;
  };

  template<size_t Stride> struct layout_left_padded;

  template<extents_like Extents, size_t Stride>
  struct _CUTILE_DECLSPEC_EMPTY_BASES layout_left_padded_mapping : private detail::padded_extents<Extents, 0, Stride> {
  private:
    using base = detail::padded_extents<Extents, 0, Stride>;
    
  public:
    static_assert(!detail::is_cv_qualified_v<Extents>, "In cuda::tiles::layout_left_padded_mapping<E>, E may not be cv-qualified.");

    using extents_type = Extents;
    using rank_type = typename extents_type::rank_type;
    using index_type = typename extents_type::index_type;
    using layout_type = layout_left_padded<Stride>;

    template<typename T>
    requires detail::is_convertible_v<T, index_type>
    __tile__ __host__ __device__ explicit layout_left_padded_mapping(extents_type const& extents, T stride) noexcept
      : base{extents, index_type(stride)} {
    }

    __tile__ __host__ __device__ static constexpr bool is_always_strided() noexcept { return true; }
    
    __tile__ __host__ __device__ static constexpr size_t static_stride(rank_type dim) noexcept {
      return layout_left_mapping<base>::static_stride(dim);
    }

    __tile__ __host__ __device__ index_type stride(rank_type dim) const noexcept {
      return layout_left_mapping<base>{*this}.stride(dim);
    }

    __tile__ __host__ __device__ extents_type const& extents() const noexcept {
      return this->base::underlying_extents();
    }
  };

  template<extents_like Extents, typename Stride> layout_left_padded_mapping(Extents const&, Stride)
     -> layout_left_padded_mapping<Extents, detail::extent_value_or_dynamic<detail::remove_cv_t<Stride>>>;

  template<size_t Stride> struct layout_left_padded {
    template<extents_like E> using mapping = layout_left_padded_mapping<detail::remove_cv_t<E>, Stride>;
  };

  template<typename T> constexpr inline bool enable_contiguous_accessor_policy = false;

  namespace detail {
    template<typename T> constexpr inline bool is_accessor_policy_v =
      is_copyable_v<T>
      && requires {
        requires scalar<typename T::element_type>;
        requires is_same_v<typename T::data_handle_type, typename T::element_type*>;
        requires is_same_v<typename T::reference, typename T::element_type&>;
    } && enable_contiguous_accessor_policy<T>;
  } // namespace detail

  template<typename T> concept accessor_policy = detail::is_accessor_policy_v<T>;

  template<scalar E> struct default_accessor {
    using element_type = E;
    using data_handle_type = E*;
    using reference = E&;
  };

  template<typename E> constexpr inline bool enable_contiguous_accessor_policy<default_accessor<E>> = true;

  namespace detail {
    template<typename T> constexpr inline bool is_tensor_span_like_v =
      is_copyable_v<T>
      && requires {
        requires !is_cv_qualified_v<typename T::mapping_type>;
        requires layout_mapping<typename T::mapping_type>;
        requires is_same_v<typename T::extents_type, typename T::mapping_type::extents_type>;
        requires is_same_v<typename T::layout_type, typename T::mapping_type::layout_type>;
        requires !is_cv_qualified_v<typename T::accessor_type>;
        requires accessor_policy<typename T::accessor_type>;
        requires is_same_v<typename T::element_type, typename T::accessor_type::element_type>;
        requires is_same_v<typename T::value_type, remove_cv_t<typename T::element_type>>;
        requires is_same_v<typename T::index_type, typename T::extents_type::index_type>;
        requires is_same_v<typename T::rank_type, typename T::extents_type::rank_type>;
        requires is_same_v<typename T::data_handle_type, typename T::accessor_type::data_handle_type>;
        requires is_same_v<typename T::reference, typename T::accessor_type::reference>;
        { declval<T const&>().data_handle()  } -> same_as<typename T::data_handle_type>;
        { declval<T const&>().mapping()  } -> same_as<typename T::mapping_type const&>;
        { declval<T const&>().accessor()  } -> same_as<typename T::accessor_type const&>;
      };
  } // namespace detail
  
  template<typename T> concept tensor_span_like = detail::is_tensor_span_like_v<detail::remove_cv_t<T>>;
  
  template<scalar E, extents_like Extents, typename Layout = layout_right, accessor_policy Accessor = default_accessor<E>>
  requires (layout_mapping<typename Layout::template mapping<Extents>>
         && same_as<E, typename Accessor::element_type>)
  struct _CUTILE_DECLSPEC_EMPTY_BASES tensor_span : private Layout::template mapping<Extents>, Accessor  {
    static_assert(!detail::is_cv_qualified_v<Extents> && !detail::is_cv_qualified_v<Layout> && !detail::is_cv_qualified_v<Accessor>,
                  "In cuda::tiles::tensor_span<E, Exts, L, A>, template arguments Exts, L, and A may not be cv-qualified.");
    
    using element_type = E;
    using value_type = detail::remove_cv_t<element_type>;
    using layout_type = Layout;
    using mapping_type = layout_type::template mapping<Extents>;
    using extents_type = typename mapping_type::extents_type;
    using index_type = typename extents_type::index_type;
    using rank_type = typename extents_type::rank_type;
    using accessor_type = Accessor;
    using data_handle_type = typename accessor_type::data_handle_type;
    using reference = typename accessor_type::reference;

    __tile__ __host__ __device__ constexpr tensor_span(data_handle_type handle, mapping_type const& mapping, accessor_type const& accessor = {}) noexcept
      : mapping_type{mapping}, accessor_type{accessor}, handle_{handle} {
    }

    __tile__ __host__ __device__ constexpr
    tensor_span(data_handle_type handle, extents_type const& extents, layout_type const& = {}, accessor_type const& accessor = {}) noexcept
      requires detail::is_constructible_v<mapping_type, extents_type const&>
      : mapping_type{extents}, accessor_type{accessor}, handle_{handle} {
    }
    
    __tile__ __host__ __device__ constexpr extents_type const& extents() const noexcept {
      return this->mapping_type::extents();
    }

    __tile__ __host__ __device__ constexpr index_type extent(rank_type idx) const noexcept {
      return extents().extent(idx);
    }

    __tile__ __host__ __device__ static constexpr size_t static_extent(rank_type idx) noexcept {
      return [=](){ return extents_type::static_extent(idx); }();
    }

    __tile__ __host__ __device__ static constexpr rank_type rank() noexcept {
      return [=](){ return extents_type::rank(); }();
    }

    __tile__ __host__ __device__ static constexpr rank_type rank_dynamic() noexcept {
      return [=](){ return extents_type::rank_dynamic(); }();
    }

    __tile__ __host__ __device__ constexpr mapping_type const& mapping() const noexcept {
      return static_cast<mapping_type const&>(*this);
    }

    __tile__ __host__ __device__ constexpr accessor_type const& accessor() const noexcept {
      return static_cast<accessor_type const&>(*this);
    }

    __tile__ __host__ __device__ constexpr auto data_handle() const noexcept {
      return handle_;
    }

  private:
    data_handle_type handle_;
  };

  template<typename T, typename M, typename A = default_accessor<T>> tensor_span(T*, M const&, A const& = {}) -> tensor_span<T, typename M::extents_type, typename M::layout_type, A>;
  template<typename T, typename E, typename L = layout_right, typename A = default_accessor<T>> tensor_span(T*, E const&, L const& = {}, A const& = {}) -> tensor_span<T, E, L, A>;

  enum class __tile_builtin__ view_padding : unsigned char {
    zero = 0,
    negative_zero = 1,
    positive_inf = 2,
    negative_inf = 3,
    nan = 4
  };

  namespace detail {
    template<view_padding Value> constexpr inline bool is_view_padding_enumerator_v = Value <= view_padding::nan;
  } // namespace detail

  template<view_padding Value> 
  requires detail::is_view_padding_enumerator_v<Value>
  struct view_padding_constant {
    using type = view_padding_constant;
    using value_type = view_padding;
      
    static constexpr view_padding value = Value;
    __tile__ __host__ __device__ constexpr operator view_padding() const noexcept {
    return value;
  }
    __tile__ __host__ __device__ constexpr view_padding operator()() const noexcept {
    return value;
  }
  };

  __tile__ __host__ __device__ constexpr view_padding default_view_padding() noexcept {
      return view_padding::zero;
  }

  using view_padding_zero_t = view_padding_constant<view_padding::zero>;
  using view_padding_negative_zero_t = view_padding_constant<view_padding::negative_zero>;
  using view_padding_positive_inf_t = view_padding_constant<view_padding::positive_inf>;
  using view_padding_negative_inf_t = view_padding_constant<view_padding::negative_inf>;
  using view_padding_nan_t = view_padding_constant<view_padding::nan>;
  using default_view_padding_t = view_padding_constant<default_view_padding()>;

  namespace detail {
    template<typename, typename> struct make_strides_specialization_from_strides { };
    template<typename S, size_t... Seq>
    struct make_strides_specialization_from_strides<S, detail::index_sequence<Seq...>> {
      using type = strides<typename S::stride_type, S::static_stride(Seq)...>;
    };

    template<typename S> using make_strides_specialization_from_strides_t =
      typename make_strides_specialization_from_strides<S, detail::make_index_sequence<S::rank()>>::type;

    template<typename, typename> struct make_strides_specialization_from_mapping { };
    template<typename M, size_t... Seq>
    struct make_strides_specialization_from_mapping<M, detail::index_sequence<Seq...>> {
      using type = strides<typename M::index_type, M::static_stride(Seq)...>;
    };

    template<typename M> using make_strides_specialization_from_mapping_t =
      typename make_strides_specialization_from_mapping<M, detail::make_index_sequence<M::extents_type::rank()>>::type;

    template<typename, typename> struct make_offsets_specialization_from_offsets { };
    template<typename O, size_t... Seq>
    struct make_offsets_specialization_from_offsets<O, detail::index_sequence<Seq...>> {
      using type = offsets<typename O::offset_type, O::static_offset(Seq)...>;
    };

    template<typename O> using make_offsets_specialization_from_offsets_t =
      typename make_offsets_specialization_from_offsets<O, detail::make_index_sequence<O::rank()>>::type;

    __tile__ __host__ __device__ consteval bool compatible_fp_view_padding(view_padding pad, fp_kind fp) {
      using enum fp_kind;
    
      switch(pad) {
      case view_padding::zero:
        return fp != e8m0;
      case view_padding::negative_zero:
        return fp != e8m0 && fp != ue5m3 && fp != e2m1;
      case view_padding::positive_inf:
        return fp != e4m3 && fp != ue5m3 && fp != e8m0 && fp != e2m1;
      case view_padding::negative_inf:
        return fp != e4m3 && fp != ue5m3 && fp != e8m0 && fp != e2m1;
      case view_padding::nan:
        return fp != e2m1;
      }

      return false;
    }
    
    template<view_padding Pad, typename Tile>
    constexpr inline bool compatible_view_padding_v = is_view_padding_enumerator_v<Pad> &&
                                                      ((floating_point_tile<Tile> && compatible_fp_view_padding(Pad, fp_kind_v<tile_element_t<Tile>>)) ||
                                                       (integral_tile<Tile> && (Pad == view_padding::zero)) ||
                                                       (pointer_tile<Tile> && (Pad == view_padding::zero)));

    template<typename T> constexpr inline bool is_32_or_64_bit_integral_scalar_v =
      integral_scalar<T> && ((sizeof(T) * TILE_CHAR_BIT == 32) || (sizeof(T) * TILE_CHAR_BIT == 64));
    
    template<typename T> constexpr inline bool atomic_compare_exchange_constraints_v =
      is_32_or_64_bit_integral_scalar_v<T> ||
      is_float64_v<T> || is_float32_v<T>;

    template<typename T> constexpr inline bool atomic_add_sub_constraints_v =
      is_32_or_64_bit_integral_scalar_v<T> ||
      is_float64_v<T> || is_float32_v<T> || is_float16_v<T>
#if !defined(__CUDA_ARCH__) || (__CUDA_ARCH__ >= 900)
      || is_bfloat16_v<T>      
#endif      
      ;

    template<typename T> constexpr inline bool atomic_xchg_constraints_v =
      is_32_or_64_bit_integral_scalar_v<T> ||
      is_float64_v<T> || is_float32_v<T>;
  } // namespace detail
  

  template<typename T> concept storeable_tensor_span = tensor_span_like<T> && 
						       !detail::is_const_v<typename T::element_type> && 
						       !detail::is_volatile_v<typename T::element_type>;
  
  template<tensor_span_like Span, tile_shape TileShape>
  requires (TileShape::rank() == Span::extents_type::rank())
  struct __tile_builtin__ partition_view {
    static_assert(!detail::is_cv_qualified_v<TileShape> && !detail::is_cv_qualified_v<Span>,
           "In cuda::tiles::partition_view<Span, Shape>, template arguments Shape and Span may not be cv-qualified.");
    
    using span_type = Span;
    using index_type = typename span_type::index_type;
    using rank_type = span_type::rank_type;
    using element_type = typename span_type::element_type;
    using value_type = typename span_type::value_type;
    using view_shape_type = TileShape;
    using view_tile_type = tile<value_type, view_shape_type>;
        
    __tile__ __host__ __device__ constexpr partition_view(span_type _span, view_shape_type) noexcept : tspan{_span} { };

    __tile__ __host__ __device__ constexpr span_type const& span() const noexcept { return tspan; }

  private:
    template<typename... Idx> static constexpr inline bool compatible_indices_v
      = sizeof...(Idx) == tile_rank_v<view_tile_type> && (scalar_convertible_to<Idx, index_type> && ...);

    using __extents_t = detail::make_extents_specialization_from_extents_t<typename span_type::extents_type>;
    using __strides_t = detail::make_strides_specialization_from_mapping_t<typename span_type::mapping_type>;

    using stride_type = typename __strides_t::stride_type;

    using __get_extent_t = index_type (*)(partition_view const& view, size_t idx);
    using __get_stride_t = stride_type (*)(partition_view const& view, size_t idx);
    using __get_base_ptr_t = element_type* (*)(partition_view const& view);
    
    static __tile__ index_type __get_extent(partition_view const& view, size_t idx) {
      return view.span().mapping().extents().extent((rank_type)idx);
    };
    
    static __tile__ stride_type __get_stride(partition_view const& view, size_t idx) {
      return view.span().mapping().stride((rank_type)idx);
    };

    static __tile__ element_type* __get_base_ptr(partition_view const& view) {
      return view.span().data_handle();
    };

    template<auto> struct __materialize { };
    using __materialize_get_extent_t = __materialize<&__get_extent>;
    using __materialize_get_stride_t = __materialize<&__get_stride>;
    using __materialize_get_base_ptr_t = __materialize<&__get_base_ptr>;

  public:
    template<typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load(Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires (compatible_indices_v<Idx...> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load_masked(Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires (compatible_indices_v<Idx...> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load_masked(view_padding_constant<Padding>, Idx... idx) const noexcept;

    template<memory_order Order,  
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(Idx... idx) const noexcept;
    
    template<memory_order Order,  
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,  
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;

    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(Idx... idx) const noexcept;
    
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>, Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,  
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>,
						  memory_order_constant<Order>,
						  Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>  && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>,
						  memory_order_constant<Order>,
						  thread_scope_constant<Scope>,
						  Idx... idx) const noexcept;

    template<tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void store(Value tile, Idx... idx) const noexcept;

    template<tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void store_masked(Value tile, Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, Idx... idx) const noexcept;
    
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, Idx... idx) const noexcept;
    
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, 
       memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;  


    template<memory_order Order,
	     thread_scope Scope, 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires compatible_indices_v<Idx...> 
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_and(Value tile, 
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope, 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr> 
    requires compatible_indices_v<Idx...> 
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_and_masked(Value tile, 
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_or(Value tile,
                                    memory_order_constant<Order>,
                                    thread_scope_constant<Scope>,
                                    Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_or_masked(Value tile,
                                           memory_order_constant<Order>,
                                           thread_scope_constant<Scope>,
                                           Idx... idx) const noexcept;


    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_xor(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_xor_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_max(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_max_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_min(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_min_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_add(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_add_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_sub(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_sub_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;
  private:
    span_type tspan;
  };

  namespace detail {
    template<typename T> struct zero_offsets_of_rank { };
    template<size_t... Seq> struct zero_offsets_of_rank<index_sequence<Seq...>> {
      using type = offsets<int32_t, (ptrdiff_t(Seq * 0))...>;
    };
    
    template<auto N> using zero_offsets_of_rank_t = typename zero_offsets_of_rank<make_index_sequence<size_t(N)>>::type;

    template<typename T, typename U> constexpr inline bool all_static_strides_helper_v = false;
    template<typename T, size_t... Seq> constexpr inline bool all_static_strides_helper_v<T, index_sequence<Seq...>> =
      ((T::static_stride(Seq) != dynamic_stride) && ...);
    
    template<typename T> constexpr inline bool all_static_strides_v = all_static_strides_helper_v<T, make_index_sequence<T::rank()>>;

    template<typename T, typename U> constexpr inline bool all_positive_strides_helper_v = false;
    template<typename T, size_t... Seq> constexpr inline bool all_positive_strides_helper_v<T, index_sequence<Seq...>> =
      ((T::static_stride(Seq) > 0) && ...);
    
    template<typename T> constexpr inline bool all_positive_strides_v = all_positive_strides_helper_v<T, make_index_sequence<T::rank()>>;
    
    template<typename Span, typename TileShape, typename Strides, typename Offsets>
    constexpr inline bool strided_view_constraints_v =
      (TileShape::rank() == Span::extents_type::rank()) &&
      (TileShape::rank() == Strides::rank()) &&
      (TileShape::rank() == Offsets::rank()) &&
      is_same_v<typename Strides::stride_type, typename Span::index_type> &&
      is_same_v<Offsets, zero_offsets_of_rank_t<TileShape::rank()>> &&
      all_static_strides_v<Strides> &&
      all_positive_strides_v<Strides>;
  }

  template<tensor_span_like Span, tile_shape TileShape,
           strides_like Strides, offsets_like Offsets = detail::zero_offsets_of_rank_t<TileShape::rank()>>
  requires detail::strided_view_constraints_v<Span, TileShape, Strides, Offsets>
  struct _CUTILE_DECLSPEC_EMPTY_BASES __tile_builtin__ strided_view : private Strides, private Offsets {
    static_assert(!detail::is_cv_qualified_v<TileShape>
                  && !detail::is_cv_qualified_v<Span>
                  && !detail::is_cv_qualified_v<Strides>
                  && !detail::is_cv_qualified_v<Offsets>,
           "In cuda::tiles::strided_view<Span, Shape, Strides>, template arguments Span, Shape, and Strides may not be cv-qualified.");
    
    using span_type = Span;
    using index_type = typename span_type::index_type;
    using rank_type = span_type::rank_type;
    using element_type = typename span_type::element_type;
    using value_type = typename span_type::value_type;
    using view_shape_type = TileShape;
    using view_tile_type = tile<value_type, view_shape_type>;

  private:
    using strides_base = Strides;
    using offsets_base = Offsets;
    
  public:
        
    __tile__ __host__ __device__ constexpr strided_view(span_type _span, view_shape_type, Strides strides, Offsets offsets = {}) noexcept
      : strides_base{strides}, offsets_base{offsets}, tspan{_span} {
    };
    
    __tile__ __host__ __device__ constexpr span_type const& span() const noexcept { return tspan; }

  private:
    template<typename... Idx> static constexpr inline bool compatible_indices_v
      = sizeof...(Idx) == tile_rank_v<view_tile_type> && (scalar_convertible_to<Idx, index_type> && ...);

    using __extents_t = detail::make_extents_specialization_from_extents_t<typename span_type::extents_type>;
    using __strides_t = detail::make_strides_specialization_from_mapping_t<typename span_type::mapping_type>;

    using __chunk_strides_t = detail::make_strides_specialization_from_strides_t<Strides>;
    using __offsets_t = detail::make_offsets_specialization_from_offsets_t<Offsets>;

    using stride_type = typename __strides_t::stride_type;
    using chunk_stride_type = typename __chunk_strides_t::stride_type;
    using offset_type = typename __offsets_t::offset_type;

    using __get_extent_t = index_type (*)(strided_view const& view, size_t idx);
    using __get_stride_t = stride_type (*)(strided_view const& view, size_t idx);
    using __get_chunk_stride_t = chunk_stride_type (*)(strided_view const& view, size_t idx);
    using __get_offset_t = offset_type (*)(strided_view const& view, size_t idx);
    using __get_base_ptr_t = element_type* (*)(strided_view const& view);
    
    static __tile__ index_type __get_extent(strided_view const& view, size_t idx) {
      return view.span().mapping().extents().extent((rank_type)idx);
    };
    
    static __tile__ stride_type __get_stride(strided_view const& view, size_t idx) {
      return view.span().mapping().stride((rank_type)idx);
    };

    static __tile__ chunk_stride_type __get_chunk_stride(strided_view const& view, size_t idx) {
      return static_cast<strides_base const&>(view).stride((typename offsets_base::rank_type)idx);
    };

    static __tile__ offset_type __get_offset(strided_view const& view, size_t idx) {
      return static_cast<offsets_base const&>(view).offset((typename offsets_base::rank_type)idx);
    };

    static __tile__ element_type* __get_base_ptr(strided_view const& view) {
      return view.span().data_handle();
    };

    template<auto> struct __materialize { };
    using __materialize_get_extent_t = __materialize<&__get_extent>;
    using __materialize_get_stride_t = __materialize<&__get_stride>;
    using __materialize_get_chunk_stride_t = __materialize<&__get_chunk_stride>;
    using __materialize_get_offset_t = __materialize<&__get_offset>;
    using __materialize_get_base_ptr_t = __materialize<&__get_base_ptr>;

  public:
    template<typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load(Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load_masked(Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load_masked(view_padding_constant<Padding>, Idx... idx) const noexcept;

    template<memory_order Order,  
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(Idx... idx) const noexcept;
    
    template<memory_order Order,  
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,  
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;

    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(Idx... idx) const noexcept;
    
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>, Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,  
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>,
						  memory_order_constant<Order>,
						  Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>  && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>,
						  memory_order_constant<Order>,
						  thread_scope_constant<Scope>,
						  Idx... idx) const noexcept;

    template<tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void store(Value tile, Idx... idx) const noexcept;

    template<tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void store_masked(Value tile, Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, Idx... idx) const noexcept;
    
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, Idx... idx) const noexcept;
    
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, 
       memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;  


    template<memory_order Order,
	     thread_scope Scope, 
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...> 
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_and(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_and_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_or(Value tile,
                                    memory_order_constant<Order>,
                                    thread_scope_constant<Scope>,
                                    Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_or_masked(Value tile,
                                           memory_order_constant<Order>,
                                           thread_scope_constant<Scope>,
                                           Idx... idx) const noexcept;


    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_xor(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_xor_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_max(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_max_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_min(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_min_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_add(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_add_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope,
	     tile_like Value,
	     typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_sub(Value tile,
                                     memory_order_constant<Order>,
                                     thread_scope_constant<Scope>,
                                     Idx... idx) const noexcept;

    template<memory_order Order,
             thread_scope Scope,
             tile_like Value,
             typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
             typename = __strides_t,
             typename = __chunk_strides_t,
             typename = __offsets_t,
             __get_extent_t = &__get_extent,
             __get_stride_t = &__get_stride,
             __get_base_ptr_t = &__get_base_ptr,
             __get_chunk_stride_t = &__get_chunk_stride,
             __get_offset_t = &__get_offset>
    requires compatible_indices_v<Idx...>
          && (Order == memory_order::relaxed)
          && (Scope == thread_scope::device || Scope == thread_scope::block)
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
          && detail::atomic_add_sub_constraints_v<tile_element_t<view_tile_type>>
    __tile_builtin__ void atomic_sub_masked(Value tile,
                                            memory_order_constant<Order>,
                                            thread_scope_constant<Scope>,
                                            Idx... idx) const noexcept;
  private:
    span_type tspan;
  };

  template<typename T, typename U, typename S, typename O = detail::zero_offsets_of_rank_t<U::rank()>>
  strided_view(T, U, S, O = {}) -> strided_view<T, U, S, O>;

  namespace detail {
    template<typename Span, typename Shape, size_t Dim>
    constexpr inline bool gather_scatter_view_constraints_v =
      (Span::extents_type::rank() == Shape::rank()) &&
      Dim < Span::extents_type::rank();
  }

  template<tensor_span_like Span, tile_shape TileShape, size_t Dim>
  requires detail::gather_scatter_view_constraints_v<Span, TileShape, Dim>
  struct __tile_builtin__ gather_scatter_view {
    
    static_assert(!detail::is_cv_qualified_v<TileShape>
                  && !detail::is_cv_qualified_v<Span>,
           "In cuda::tiles::gather_scatter_view<Span, Shape, Dim>, template arguments Span and Shape may not be cv-qualified.");
    
    using span_type = Span;
    using index_type = typename span_type::index_type;
    using rank_type = span_type::rank_type;
    using element_type = typename span_type::element_type;
    using value_type = typename span_type::value_type;
    using view_shape_type = TileShape;
    using view_tile_type = tile<value_type, view_shape_type>;

    template<typename T>
    requires detail::is_convertible_v<T, size_t>
    __tile__ __host__ __device__ constexpr gather_scatter_view(span_type _span, view_shape_type, T) noexcept
      : tspan{_span} {
    };
    
    __tile__ __host__ __device__ constexpr span_type const& span() const noexcept { return tspan; }

  private:
    using __gather_scatter_index_type = tile<index_type, shape<view_shape_type::static_extent(Dim)>>;
    
    template<typename Idx, size_t Seq> static constexpr inline bool compatible_index_v =
      (Seq == Dim && tile_convertible_to<Idx, __gather_scatter_index_type>) ||
      (Seq != Dim && scalar_convertible_to<Idx, index_type>);

    template<typename, typename... Idx> struct compatible_indices_helper { };

    template<typename... Idx, size_t... Seq>
    struct compatible_indices_helper<detail::index_sequence<Seq...>, Idx...> {
      static constexpr inline bool value =  (compatible_index_v<Idx, Seq> && ...);
    };

    template<typename... Idx> static constexpr inline bool compatible_indices_v
      = sizeof...(Idx) == tile_rank_v<view_tile_type> && compatible_indices_helper<detail::make_index_sequence<sizeof...(Idx)>, Idx...>::value;

    using __extents_t = detail::make_extents_specialization_from_extents_t<typename span_type::extents_type>;
    using __strides_t = detail::make_strides_specialization_from_mapping_t<typename span_type::mapping_type>;

    using stride_type = typename __strides_t::stride_type;

    using __get_extent_t = index_type (*)(gather_scatter_view const& view, size_t idx);
    using __get_stride_t = stride_type (*)(gather_scatter_view const& view, size_t idx);
    using __get_base_ptr_t = element_type* (*)(gather_scatter_view const& view);
    
    static __tile__ index_type __get_extent(gather_scatter_view const& view, size_t idx) {
      return view.span().mapping().extents().extent((rank_type)idx);
    };
    
    static __tile__ stride_type __get_stride(gather_scatter_view const& view, size_t idx) {
      return view.span().mapping().stride((rank_type)idx);
    };

    static __tile__ element_type* __get_base_ptr(gather_scatter_view const& view) {
      return view.span().data_handle();
    };

    template<auto> struct __materialize { };
    using __materialize_get_extent_t = __materialize<&__get_extent>;
    using __materialize_get_stride_t = __materialize<&__get_stride>;
    using __materialize_get_base_ptr_t = __materialize<&__get_base_ptr>;
    
  public:
    template<typename... Idx,
             typename = view_tile_type,
             typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load(Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires (compatible_indices_v<Idx...> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load_masked(Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires (compatible_indices_v<Idx...> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type load_masked(view_padding_constant<Padding>, Idx... idx) const noexcept;

    template<memory_order Order,  
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(Idx... idx) const noexcept;
    
    template<memory_order Order,  
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,  
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load(memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;

    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(Idx... idx) const noexcept;
    
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
             thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>, Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,  
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order> && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>,
						  memory_order_constant<Order>,
						  Idx... idx) const noexcept;
  
    template<view_padding Padding = default_view_padding(),
	     memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires (compatible_indices_v<Idx...> && read_memory_order<Order>  && detail::compatible_view_padding_v<Padding, view_tile_type>)
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ view_tile_type atomic_load_masked(view_padding_constant<Padding>,
						  memory_order_constant<Order>,
						  thread_scope_constant<Scope>,
						  Idx... idx) const noexcept;

    template<tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void store(Value tile, Idx... idx) const noexcept;

    template<tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void store_masked(Value tile, Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, Idx... idx) const noexcept;
    
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store(Value tile, memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;

    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, Idx... idx) const noexcept;
    
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type> 
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, memory_order_constant<Order>, Idx... idx) const noexcept;
  
    template<memory_order Order,
	     thread_scope Scope = default_thread_scope(), 
	     tile_like Value,
	     typename... Idx,
	     typename = view_tile_type,
	     typename = __extents_t,
	     typename = __strides_t,
	     __get_extent_t = &__get_extent,
	     __get_stride_t = &__get_stride,
	     __get_base_ptr_t = &__get_base_ptr,
             size_t = Dim,
             typename = __gather_scatter_index_type>
    requires compatible_indices_v<Idx...> 
          && write_memory_order<Order> 
          && non_narrowing_tile_convertible_to<Value, view_tile_type>
          && storeable_tensor_span<span_type>
    __applicable_tile_hints__("latency", "allow_tma")
    __tile_builtin__ void atomic_store_masked(Value tile, 
       memory_order_constant<Order>, thread_scope_constant<Scope>, Idx... idx) const noexcept;    
  
  private:
    span_type tspan;
  };

  template<typename Span, typename Shape, integral auto Dim>
  gather_scatter_view(Span, Shape, integral_constant<Dim>) -> gather_scatter_view<Span, Shape, size_t(Dim)>;

  template<memory_order Order,
	   thread_scope Scope = default_thread_scope(),
	   storeable_tile Ptrs, 
	   non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Cmp, 
	   non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Value>
  requires detail::atomic_compare_exchange_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_compare_exchange(Ptrs ptrs, 
	                                                     Cmp cmp,
	                                                     Value value,
	                                                     memory_order_constant<Order> = {}, 
	                                                     thread_scope_constant<Scope> = {}) noexcept;
    
  template<memory_order Order, 
	   thread_scope Scope = default_thread_scope(), 
	   storeable_tile Ptrs, 
	   non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Cmp, 
	   non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Value, 
	   bool_tile_convertible Mask> 
  requires detail::atomic_compare_exchange_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_compare_exchange_masked(Ptrs ptrs, 
	                                                            Cmp cmp, 
	                                                            Value value, 
	                                                            Mask mask, 
	                                                            memory_order_constant<Order> = {}, 
	                                                            thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_and(Ptrs ptrs, 
                                                Values values,
                                                memory_order_constant<Order> = {},
                                                thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_and_masked(Ptrs ptrs, 
                                                       Values values,
	                                               Mask mask,
                                                       memory_order_constant<Order> = {},
                                                       thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_or(Ptrs ptrs, 
                                               Values values,
                                               memory_order_constant<Order> = {},
                                               thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_or_masked(Ptrs ptrs, 
                                                      Values values,
	                                              Mask mask,
                                                      memory_order_constant<Order> = {},
                                                      thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_xor(Ptrs ptrs, 
                                                Values values,
                                                memory_order_constant<Order> = {},
                                                thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_xor_masked(Ptrs ptrs, 
                                                       Values values,
	                                               Mask mask,
                                                       memory_order_constant<Order> = {},
                                                       thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_max(Ptrs ptrs, 
                                                Values values,
                                                memory_order_constant<Order> = {},
                                                thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_max_masked(Ptrs ptrs, 
                                                       Values values,
	                                               Mask mask,
                                                       memory_order_constant<Order> = {},
                                                       thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_min(Ptrs ptrs, 
                                                Values values,
                                                memory_order_constant<Order> = {},
                                                thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::is_32_or_64_bit_integral_scalar_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_min_masked(Ptrs ptrs, 
                                                       Values values,
	                                               Mask mask,
                                                       memory_order_constant<Order> = {},
                                                       thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::atomic_add_sub_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_add(Ptrs ptrs, 
                                                Values values,
                                                memory_order_constant<Order> = {},
                                                thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::atomic_add_sub_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_add_masked(Ptrs ptrs, 
                                                       Values values,
	                                               Mask mask,
                                                       memory_order_constant<Order> = {},
                                                       thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::atomic_add_sub_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_sub(Ptrs ptrs, 
                                                Values values,
                                                memory_order_constant<Order> = {},
                                                thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::atomic_add_sub_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_sub_masked(Ptrs ptrs, 
                                                       Values values,
	                                               Mask mask,
                                                       memory_order_constant<Order> = {},
                                                       thread_scope_constant<Scope> = {}) noexcept;

  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values>
  requires detail::atomic_xchg_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_xchg(Ptrs ptrs, 
                                                 Values values,
                                                 memory_order_constant<Order> = {},
                                                 thread_scope_constant<Scope> = {}) noexcept;
  
  template<memory_order Order, 
           thread_scope Scope = default_thread_scope(),
           storeable_tile Ptrs,
           non_narrowing_tile_convertible_to<tile_load_t<Ptrs>> Values,
	   bool_tile_convertible Mask>
  requires detail::atomic_xchg_constraints_v<tile_element_t<tile_load_t<Ptrs>>>
        && broadcastable_to<Mask, tile_shape_t<Ptrs>>
  __tile_builtin__ tile_load_t<Ptrs> atomic_xchg_masked(Ptrs ptrs, 
                                                        Values values,
	                                                Mask mask,
                                                        memory_order_constant<Order> = {},
                                                        thread_scope_constant<Scope> = {}) noexcept;

_CUTILE_END_CUDA_TILES_NAMESPACE

#undef __applicable_tile_hints__
#undef _CUTILE_DECLSPEC_EMPTY_BASES
#undef _CUTILE_BEGIN_CUDA_TILES_NAMESPACE
#undef _CUTILE_END_CUDA_TILES_NAMESPACE
#endif
