//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BEAST_ERROR_HPP
#define BEAST_ERROR_HPP

#include <beast/core/detail/config.hpp>
#include <system_error>

namespace beast {

/// The type of error code used by the library
using error_code = std::error_code;

/// The type of system error thrown by the library
using system_error = std::system_error;

/// The type of error category used by the library
using error_category = std::error_category;

/// A function to return the generic error category used by the library
#if BEAST_DOXYGEN
error_category const&
generic_category();
#else
using std::generic_category;
#endif

/// A function to return the system error category used by the library
#if BEAST_DOXYGEN
error_category const&
system_category();
#else
using std::system_category;
#endif

/// The type of error condition used by the library
using error_condition = std::error_condition;

/// The set of constants used for cross-platform error codes
#if BEAST_DOXYGEN
enum errc{};
#else
using std::errc;
#endif

} // beast

#endif
