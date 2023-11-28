#pragma once

#include <array>
#include <type_traits>

#ifndef __cpp_lib_to_underlying
	#define __cpp_lib_to_underlying 202102L

namespace std {

template <typename E>
constexpr typename underlying_type<E>::type to_underlying(E e) noexcept {
	return static_cast<typename underlying_type<E>::type>(e);
}

} // namespace std

#endif

#ifndef __cpp_lib_to_array
	#define __cpp_lib_to_array 201907L

namespace std {

namespace detail {

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(T (&&a)[N], std::index_sequence<I...>) {
	return {{std::move(a[I])...}};

} // namespace detail

} // namespace detail

template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
	return detail::to_array_impl(std::move(a), std::make_index_sequence<N> {});
}

} // namespace std

#endif

namespace std {

namespace detail {
// To allow ADL with custom begin/end
using std::begin;
using std::end;

template <typename T>
auto is_iterable_impl(int) -> decltype(begin(std::declval<T &>()) != end(std::declval<T &>()), // begin/end and operator !=
	void(), // Handle evil operator ,
	++std::declval<decltype(begin(std::declval<T &>())) &>(), // operator ++
	void(*begin(std::declval<T &>())), // operator*
	std::true_type {});

template <typename T>
std::false_type is_iterable_impl(...);

} // namespace detail

template <typename T>
using is_iterable = decltype(detail::is_iterable_impl<T>(0));

} // namespace std
