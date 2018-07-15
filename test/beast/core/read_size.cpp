//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <beast/core/read_size.hpp>

#include <beast/core/flat_buffer.hpp>
#include <beast/core/flat_static_buffer.hpp>
#include <beast/core/multi_buffer.hpp>
#include <beast/core/static_buffer.hpp>
#include <beast/unit_test/suite.hpp>

#include <asio/streambuf.hpp>

namespace beast {

class read_size_test : public beast::unit_test::suite
{
public:
    template<class DynamicBuffer>
    void
    check()
    {
        DynamicBuffer buffer;
        read_size(buffer, 65536);
        pass();
    }

    void
    run() override
    {
        check<flat_buffer>();
        check<flat_static_buffer<1024>>();
        check<multi_buffer>();
        check<static_buffer<1024>>();
        check<asio::streambuf>();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,read_size);

} // beast
