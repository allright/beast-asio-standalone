//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BEAST_WEBSOCKET_IMPL_PING_IPP
#define BEAST_WEBSOCKET_IMPL_PING_IPP

#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/type_traits.hpp>
#include <beast/core/detail/config.hpp>
#include <beast/websocket/detail/frame.hpp>
#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/coroutine.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/handler_continuation_hook.hpp>
#include <asio/handler_invoke_hook.hpp>
#include <asio/post.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace beast {
namespace websocket {

/*
    This composed operation handles sending ping and pong frames.
    It only sends the frames it does not make attempts to read
    any frame data.
*/
template<class NextLayer, bool deflateSupported>
template<class Handler>
class stream<NextLayer, deflateSupported>::ping_op
    : public asio::coroutine
{
    struct state
    {
        stream<NextLayer, deflateSupported>& ws;
        asio::executor_work_guard<decltype(std::declval<
            stream<NextLayer, deflateSupported>&>().get_executor())> wg;
        detail::frame_buffer fb;

        state(
            Handler const&,
            stream<NextLayer, deflateSupported>& ws_,
            detail::opcode op,
            ping_data const& payload)
            : ws(ws_)
            , wg(ws.get_executor())
        {
            // Serialize the control frame
            ws.template write_ping<
                flat_static_buffer_base>(
                    fb, op, payload);
        }
    };

    handler_ptr<state, Handler> d_;

public:
    static constexpr int id = 3; // for soft_mutex

    ping_op(ping_op&&) = default;
    ping_op(ping_op const&) = delete;

    template<class DeducedHandler>
    ping_op(
        DeducedHandler&& h,
        stream<NextLayer, deflateSupported>& ws,
        detail::opcode op,
        ping_data const& payload)
        : d_(std::forward<DeducedHandler>(h),
            ws, op, payload)
    {
    }

    using allocator_type =
        asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return (asio::get_associated_allocator)(d_.handler());
    }

    using executor_type = asio::associated_executor_t<
        Handler, decltype(std::declval<stream<NextLayer, deflateSupported>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (asio::get_associated_executor)(
            d_.handler(), d_->ws.get_executor());
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    bool asio_handler_is_continuation(ping_op* op)
    {
        using asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, ping_op* op)
    {
        using asio::asio_handler_invoke;
        asio_handler_invoke(
            f, std::addressof(op->d_.handler()));
    }
};

template<class NextLayer, bool deflateSupported>
template<class Handler>
void
stream<NextLayer, deflateSupported>::
ping_op<Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    ASIO_CORO_REENTER(*this)
    {
        // Maybe suspend
        if(d.ws.wr_block_.try_lock(this))
        {
            // Make sure the stream is open
            if(! d.ws.check_open(ec))
            {
                ASIO_CORO_YIELD
                asio::post(
                    d.ws.get_executor(),
                    bind_handler(std::move(*this), ec));
                goto upcall;
            }
        }
        else
        {
            // Suspend
            ASIO_CORO_YIELD
            d.ws.paused_ping_.emplace(std::move(*this));

            // Acquire the write block
            d.ws.wr_block_.lock(this);

            // Resume
            ASIO_CORO_YIELD
            asio::post(
                d.ws.get_executor(), std::move(*this));
            BOOST_ASSERT(d.ws.wr_block_.is_locked(this));

            // Make sure the stream is open
            if(! d.ws.check_open(ec))
                goto upcall;
        }

        // Send ping frame
        ASIO_CORO_YIELD
        asio::async_write(d.ws.stream_,
            d.fb.data(), std::move(*this));
        if(! d.ws.check_ok(ec))
            goto upcall;

    upcall:
        d.ws.wr_block_.unlock(this);
        d.ws.paused_close_.maybe_invoke() ||
            d.ws.paused_rd_.maybe_invoke() ||
            d.ws.paused_wr_.maybe_invoke();
        {
            auto wg = std::move(d.wg);
            d_.invoke(ec);
        }
    }
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
ping(ping_data const& payload)
{
    error_code ec;
    ping(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
ping(ping_data const& payload, error_code& ec)
{
    // Make sure the stream is open
    if(! check_open(ec))
        return;
    detail::frame_buffer fb;
    write_ping<flat_static_buffer_base>(
        fb, detail::opcode::ping, payload);
    asio::write(stream_, fb.data(), ec);
    if(! check_ok(ec))
        return;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
pong(ping_data const& payload)
{
    error_code ec;
    pong(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
pong(ping_data const& payload, error_code& ec)
{
    // Make sure the stream is open
    if(! check_open(ec))
        return;
    detail::frame_buffer fb;
    write_ping<flat_static_buffer_base>(
        fb, detail::opcode::pong, payload);
    asio::write(stream_, fb.data(), ec);
    if(! check_ok(ec))
        return;
}

template<class NextLayer, bool deflateSupported>
template<class WriteHandler>
ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_ping(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BEAST_HANDLER_INIT(
        WriteHandler, void(error_code));
    ping_op<ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code))>{
            std::move(init.completion_handler), *this,
                detail::opcode::ping, payload}();
    return init.result.get();
}

template<class NextLayer, bool deflateSupported>
template<class WriteHandler>
ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_pong(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BEAST_HANDLER_INIT(
        WriteHandler, void(error_code));
    ping_op<ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code))>{
            std::move(init.completion_handler), *this,
                detail::opcode::pong, payload}();
    return init.result.get();
}

} // websocket
} // beast

#endif
