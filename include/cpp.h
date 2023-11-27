#pragma once

#include <array>
#include <cstddef>

#ifndef __cpp_lib_to_array
	#define __cpp_lib_to_array 201907L

namespace std {

namespace detail {

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N>
to_array_impl(T (&&a)[N], std::index_sequence<I...>) {
	return {{std::move(a[I])...}};
}
} // namespace detail

template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
	return detail::to_array_impl(std::move(a), std::make_index_sequence<N> {});
}

} // namespace std

#endif //__cpp_lib_to_array

#ifndef __cpp_lib_to_underlying
	#define __cpp_lib_to_underlying 202102L

namespace std {

/// Convert an object of enumeration type to its underlying type.
template <typename _Tp>
constexpr underlying_type_t<_Tp> to_underlying(_Tp __value) noexcept {
	return static_cast<underlying_type_t<_Tp>>(__value);
}

} // namespace std

#endif // __cpp_lib_to_underlying
