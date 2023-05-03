#pragma once


#include <cstddef>
#include <array>

namespace std {

namespace detail {
 
// check for c++20 features
#ifndef __cpp_lib_to_array

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N>
    to_array_impl(T (&&a)[N], std::index_sequence<I...>) {
    return {{std::move(a[I])...}};
}
 
}
 
template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
    return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
}

#endif


#ifndef __cpp_lib_to_underlying
  /// Convert an object of enumeration type to its underlying type.
  template<typename _Tp>
    constexpr underlying_type_t<_Tp>
    to_underlying(_Tp __value) noexcept
    { return static_cast<underlying_type_t<_Tp>>(__value); }
#endif // C++23

} // namespace std
