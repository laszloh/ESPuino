#pragma once

// Helper class for CRTP (Curiously Recurring Template Pattern) as proposed by https://www.fluentcpp.com/2017/05/19/crtp-helper/
template <typename T, template <typename> class crtpType>
struct crtp {
	T &underlying() { return static_cast<T &>(*this); }
	T const &underlying() const { return static_cast<T const &>(*this); }

private:
	crtp() { }
	friend crtpType<T>;
};

#include <cstdint>

#define DEFINE_HAS_SIGNATURE(traitsName, funcName, signature)                      \
	template <typename U>                                                          \
	class traitsName {                                                             \
	private:                                                                       \
		template <typename T, T>                                                   \
		struct helper;                                                             \
		template <typename T>                                                      \
		static std::uint8_t check(helper<signature, &funcName> *);                 \
		template <typename T>                                                      \
		static std::uint16_t check(...);                                           \
                                                                                   \
	public:                                                                        \
		static constexpr bool value = sizeof(check<U>(0)) == sizeof(std::uint8_t); \
	}
