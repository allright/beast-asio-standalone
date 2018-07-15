//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <beast/core/file_stdio.hpp>

#include "file_test.hpp"

#include <beast/core/type_traits.hpp>
#include <beast/unit_test/suite.hpp>

namespace beast {

BOOST_STATIC_ASSERT(! std::is_copy_constructible<file_stdio>::value);

class file_stdio_test
    : public beast::unit_test::suite
{
public:
    void
    run()
    {
        doTestFile<file_stdio>(*this);
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,file_stdio);

} // beast
