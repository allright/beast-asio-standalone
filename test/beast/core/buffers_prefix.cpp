//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <beast/core/buffers_prefix.hpp>

#include <beast/core/buffers_suffix.hpp>
#include <beast/core/buffers_to_string.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/unit_test/suite.hpp>
#include <asio/buffer.hpp>
#include <string>

namespace beast {

BOOST_STATIC_ASSERT(
    std::is_same<asio::const_buffer, decltype(
        buffers_prefix(0,
            std::declval<asio::const_buffer>()))>::value);

BOOST_STATIC_ASSERT(
    asio::is_const_buffer_sequence<decltype(
        buffers_prefix(0,
            std::declval<asio::const_buffer>()))>::value);

BOOST_STATIC_ASSERT(
    std::is_same<asio::mutable_buffer, decltype(
        buffers_prefix(0,
            std::declval<asio::mutable_buffer>()))>::value);

class buffers_prefix_test : public beast::unit_test::suite
{
public:
    template<class ConstBufferSequence>
    static
    std::size_t
    bsize1(ConstBufferSequence const& bs)
    {
        using asio::buffer_size;
        std::size_t n = 0;
        for(auto it = bs.begin(); it != bs.end(); ++it)
            n += buffer_size(*it);
        return n;
    }

    template<class ConstBufferSequence>
    static
    std::size_t
    bsize2(ConstBufferSequence const& bs)
    {
        using asio::buffer_size;
        std::size_t n = 0;
        for(auto it = bs.begin(); it != bs.end(); it++)
            n += buffer_size(*it);
        return n;
    }

    template<class ConstBufferSequence>
    static
    std::size_t
    bsize3(ConstBufferSequence const& bs)
    {
        using asio::buffer_size;
        std::size_t n = 0;
        for(auto it = bs.end(); it != bs.begin();)
            n += buffer_size(*--it);
        return n;
    }

    template<class ConstBufferSequence>
    static
    std::size_t
    bsize4(ConstBufferSequence const& bs)
    {
        using asio::buffer_size;
        std::size_t n = 0;
        for(auto it = bs.end(); it != bs.begin();)
        {
            it--;
            n += buffer_size(*it);
        }
        return n;
    }

    template<class BufferType>
    void testMatrix()
    {
        using asio::buffer_size;
        std::string s = "Hello, world";
        BEAST_EXPECT(s.size() == 12);
        for(std::size_t x = 1; x < 4; ++x) {
        for(std::size_t y = 1; y < 4; ++y) {
        std::size_t z = s.size() - (x + y);
        {
            std::array<BufferType, 3> bs{{
                BufferType{&s[0], x},
                BufferType{&s[x], y},
                BufferType{&s[x+y], z}}};
            for(std::size_t i = 0; i <= s.size() + 1; ++i)
            {
                auto pb = buffers_prefix(i, bs);
                BEAST_EXPECT(buffers_to_string(pb) == s.substr(0, i));
                auto pb2 = pb;
                BEAST_EXPECT(buffers_to_string(pb2) == buffers_to_string(pb));
                pb = buffers_prefix(0, bs);
                pb2 = pb;
                BEAST_EXPECT(buffer_size(pb2) == 0);
                pb2 = buffers_prefix(i, bs);
                BEAST_EXPECT(buffers_to_string(pb2) == s.substr(0, i));
            }
        }
        }}
    }

    void testEmptyBuffers()
    {
        using asio::buffer_copy;
        using asio::buffer_size;
        using asio::mutable_buffer;
        auto pb0 = buffers_prefix(0, mutable_buffer{});
        BEAST_EXPECT(buffer_size(pb0) == 0);
        auto pb1 = buffers_prefix(1, mutable_buffer{});
        BEAST_EXPECT(buffer_size(pb1) == 0);
        BEAST_EXPECT(buffer_copy(pb0, pb1) == 0);

        using pb_type = decltype(pb0);
        buffers_suffix<pb_type> cb(pb0);
        BEAST_EXPECT(buffer_size(cb) == 0);
        BEAST_EXPECT(buffer_copy(cb, pb1) == 0);
        cb.consume(1);
        BEAST_EXPECT(buffer_size(cb) == 0);
        BEAST_EXPECT(buffer_copy(cb, pb1) == 0);

        auto pbc = buffers_prefix(2, cb);
        BEAST_EXPECT(buffer_size(pbc) == 0);
        BEAST_EXPECT(buffer_copy(pbc, cb) == 0);
    }

    void testIterator()
    {
        using asio::buffer_size;
        using asio::const_buffer;
        char b[3];
        std::array<const_buffer, 3> bs{{
            const_buffer{&b[0], 1},
            const_buffer{&b[1], 1},
            const_buffer{&b[2], 1}}};
        auto pb = buffers_prefix(2, bs);
        BEAST_EXPECT(bsize1(pb) == 2);
        BEAST_EXPECT(bsize2(pb) == 2);
        BEAST_EXPECT(bsize3(pb) == 2);
        BEAST_EXPECT(bsize4(pb) == 2);
    }

    void run() override
    {
        testMatrix<asio::const_buffer>();
        testMatrix<asio::mutable_buffer>();
        testEmptyBuffers();
        testIterator();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffers_prefix);

} // beast
