#pragma once

#include <array>
#include <cstddef>
#include <utility>

// std::to_array (C++20): https://en.cppreference.com/w/cpp/container/array/to_array
#ifndef __cpp_lib_to_array
	#define __cpp_lib_to_array 201907L

namespace std {
namespace detail {

template <class T, size_t N, size_t... I>
constexpr array<remove_cv_t<T>, N> to_array_impl(T (&&a)[N], index_sequence<I...>) {
	return {{move(a[I])...}};
}

} // namespace detail

template <class T, size_t N>
constexpr array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
	return detail::to_array_impl(move(a), make_index_sequence<N> {});
}

} // namespace std

#endif

// std::to_underlying (C++23): https://en.cppreference.com/w/cpp/utility/to_underlying
#ifndef __cpp_lib_to_underlying
	#define __cpp_lib_to_underlying 202102L

namespace std {

/// Convert an object of enumeration type to its underlying type.
template <typename _Tp>
constexpr underlying_type_t<_Tp> to_underlying(_Tp __value) noexcept {
	return static_cast<underlying_type_t<_Tp>>(__value);
}

} // namespace std

#endif
