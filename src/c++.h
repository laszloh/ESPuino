#pragma once

namespace std {

// define std::clamp here if we do not have access to it (since we ar enot using c++17)

#if __cplusplus < 201703L   

// clamp value n between lower & upper limit
template <typename T>
T clamp(const T& n, const T& lower, const T& upper) {
  return std::max(lower, std::min(n, upper));
}

#endif

}