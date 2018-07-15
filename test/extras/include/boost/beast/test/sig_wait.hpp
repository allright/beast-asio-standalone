//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BEAST_TEST_SIG_WAIT_HPP
#define BEAST_TEST_SIG_WAIT_HPP

#include <asio.hpp>

namespace beast {
namespace test {

/// Block until SIGINT or SIGTERM is received.
inline
void
sig_wait()
{
    asio::io_context ioc;
    asio::signal_set signals(
        ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](std::error_code const&, int)
        {
        });
    ioc.run();
}

} // test
} // beast

#endif
