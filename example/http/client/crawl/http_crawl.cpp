//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP crawl (asynchronous)
//
//------------------------------------------------------------------------------

#include "urls_large_data.hpp"

#include <beast/core.hpp>
#include <beast/http.hpp>
#include <beast/version.hpp>
#include <asio/bind_executor.hpp>
#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>

using tcp = asio::ip::tcp;       // from <asio/ip/tcp.hpp>
namespace http = beast::http;    // from <beast/http.hpp>
namespace chrono = std::chrono;         // from <chrono>

//------------------------------------------------------------------------------

// This structure aggregates statistics on all the sites
class crawl_report
{
    asio::io_context& ioc_;
    asio::strand<
        asio::io_context::executor_type> strand_;
    std::atomic<std::size_t> index_;
    std::vector<char const*> const& hosts_;
    std::size_t count_ = 0;

public:
    crawl_report(asio::io_context& ioc)
        : ioc_(ioc)
        , strand_(ioc_.get_executor())
        , index_(0)
        , hosts_(urls_large_data())
    {
    }

    // Run an aggregation function on the strand.
    // This allows synchronization without a mutex.
    template<class F>
    void
    aggregate(F const& f)
    {
        asio::post(
            strand_,
            [&, f]
            {
                f(*this);
                if(count_ % 100 == 0)
                {
                    std::cerr <<
                        "Progress: " << count_ << " of " << hosts_.size() << "\n";
                    //std::cerr << *this;
                }
                ++count_;
            });
    }

    // Returns the next host to check
    char const*
    get_host()
    {
        auto const n = index_++;
        if(n >= hosts_.size())
            return nullptr;
        return hosts_[n];
    }

    // Counts the number of timer failures
    std::size_t timer_failures = 0;

    // Counts the number of name resolution failures
    std::size_t resolve_failures = 0;

    // Counts the number of connection failures
    std::size_t connect_failures = 0;

    // Counts the number of write failures
    std::size_t write_failures = 0;

    // Counts the number of read failures
    std::size_t read_failures = 0;

    // Counts the number of success reads
    std::size_t success = 0;

    // Counts the number received of each status code
    std::map<unsigned, std::size_t> status_codes;
};

std::ostream&
operator<<(std::ostream& os, crawl_report const& report)
{
    // Print the report
    os <<
        "Crawl report\n" <<
        "   Failure counts\n" <<
        "       Timer   : " << report.timer_failures << "\n" <<
        "       Resolve : " << report.resolve_failures << "\n" <<
        "       Connect : " << report.connect_failures << "\n" <<
        "       Write   : " << report.write_failures << "\n" <<
        "       Read    : " << report.read_failures << "\n" <<
        "       Success : " << report.success << "\n" <<
        "   Status codes\n"
        ;
    for(auto const& result : report.status_codes)
        os <<
        "       " << std::setw(3) << result.first << ": " << result.second <<
                    " (" << http::obsolete_reason(static_cast<http::status>(result.first)) << ")\n";
    os.flush();
    return os;
}

//------------------------------------------------------------------------------

// Performs HTTP GET requests and aggregates the results into a report
class worker : public std::enable_shared_from_this<worker>
{
    enum
    {
        // Use a small timeout to keep things lively
        timeout = 5
    };

    crawl_report& report_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    asio::steady_timer timer_;
    asio::strand<
        asio::io_context::executor_type> strand_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;

public:
    worker(worker&&) = default;

    // Resolver and socket require an io_context
    worker(
        crawl_report& report,
        asio::io_context& ioc)
        : report_(report)
        , resolver_(ioc)
        , socket_(ioc)
        , timer_(ioc,
            (chrono::steady_clock::time_point::max)())
        , strand_(ioc.get_executor())
    {
        // Set up the common fields of the request
        req_.version(11);
        req_.method(http::verb::get);
        req_.target("/");
        req_.set(http::field::user_agent, BEAST_VERSION_STRING);
    }

    // Start the asynchronous operation
    void
    run()
    {
        // Run the timer. The timer is operated
        // continuously, this simplifies the code.
        on_timer({});

        do_get_host();
    }

    void
    on_timer(std::error_code ec)
    {
        if(ec && ec != asio::error::operation_aborted)
        {
            // Should never happen
            report_.aggregate(
            [](crawl_report& rep)
            {
                ++rep.timer_failures;
            });
            return;
        }

        // Verify that the timer really expired since the deadline may have moved.
        if(timer_.expiry() <= chrono::steady_clock::now())
        {
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
            return;
        }

        // Wait on the timer
        timer_.async_wait(
            asio::bind_executor(
                strand_,
                std::bind(
                    &worker::on_timer,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    do_get_host()
    {
        // Grab another host
        auto const host = report_.get_host();

        // nullptr means no more work
        if(! host)
        {
            timer_.cancel_one();
            return;
        }

        // The Host HTTP field is required
        req_.set(http::field::host, host);

        // Set the timer
        timer_.expires_after(chrono::seconds(timeout));

        // Set up an HTTP GET request message
        // Look up the domain name
        resolver_.async_resolve(
            host,
            "http",
            asio::bind_executor(
                strand_,
                std::bind(
                    &worker::on_resolve,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_resolve(
        std::error_code ec,
        tcp::resolver::results_type results)
    {
        if(ec)
        {
            report_.aggregate(
            [](crawl_report& rep)
            {
                ++rep.resolve_failures;
            });
            return do_get_host();
        }

        // Set the timer
        timer_.expires_after(chrono::seconds(timeout));

        // Make the connection on the IP address we get from a lookup
        asio::async_connect(
            socket_,
            results.begin(),
            results.end(),
            asio::bind_executor(
                strand_,
                std::bind(
                    &worker::on_connect,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    on_connect(std::error_code ec)
    {
        if(ec)
        {
            report_.aggregate(
            [](crawl_report& rep)
            {
                ++rep.connect_failures;
            });
            return do_get_host();
        }

        // Set the timer
        timer_.expires_after(chrono::seconds(timeout));

        // Send the HTTP request to the remote host
        http::async_write(
            socket_,
            req_,
            asio::bind_executor(
                strand_,
                std::bind(
                    &worker::on_write,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_write(
        std::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
        {
            report_.aggregate(
            [](crawl_report& rep)
            {
                ++rep.write_failures;
            });
            return do_get_host();
        }
        
        // Set the timer
        timer_.expires_after(chrono::seconds(timeout));

        // Receive the HTTP response
        http::async_read(
            socket_,
            buffer_,
            res_,
            asio::bind_executor(
                strand_,
                std::bind(
                    &worker::on_read,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_read(
        std::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
        {
            report_.aggregate(
            [](crawl_report& rep)
            {
                ++rep.read_failures;
            });
            return do_get_host();
        }

        auto const code = res_.result_int();
        report_.aggregate(
        [code](crawl_report& rep)
        {
            ++rep.success;
            ++rep.status_codes[code];
        });

        // Gracefully close the socket
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);

        // If we get here then the connection is closed gracefully

        do_get_host();
    }
};

class timer
{
    using clock_type = chrono::system_clock;

    clock_type::time_point when_;

public:
    using duration = clock_type::duration;

    timer()
        : when_(clock_type::now())
    {
    }

    duration
    elapsed() const
    {
        return clock_type::now() - when_;
    }
};

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 2)
    {
        std::cerr <<
            "Usage: http-crawl <threads>\n" <<
            "Example:\n" <<
            "    http-crawl 100 1\n";
        return EXIT_FAILURE;
    }
    auto const threads = std::max<int>(1, std::atoi(argv[1]));

    // The io_context is required for all I/O
    asio::io_context ioc{1};

    // The work keeps io_context::run from returning
    auto work = asio::make_work_guard(ioc);

    // The report holds the aggregated statistics
    crawl_report report{ioc};

    timer t;

    // Create and launch the worker threads.
    std::vector<std::thread> workers;
    workers.reserve(threads + 1);
    for(int i = 0; i < threads; ++i)
        workers.emplace_back(
        [&report]
        {
            // We use a separate io_context for each worker because
            // the asio resolver simulates asynchronous operation using
            // a dedicated worker thread per io_context, and we want to
            // do a lot of name resolutions in parallel.
            asio::io_context ioc{1};
            std::make_shared<worker>(report, ioc)->run();
            ioc.run();
        });

    // Add another thread to run the main io_context which
    // is used to aggregate the statistics
    workers.emplace_back(
    [&ioc]
    {
        ioc.run();
    });

    // Now block until all threads exit
    for(std::size_t i = 0; i < workers.size(); ++i)
    {
        auto& thread = workers[i];

        // If this is the last thread, reset the
        // work object so that it can return from run.
        if(i == workers.size() - 1)
            work.reset();

        // Wait for the thread to exit
        thread.join();
    }

    std::cout <<
        "Elapsed time:    " << chrono::duration_cast<chrono::seconds>(t.elapsed()).count() << " seconds\n";
    std::cout << report;

    return EXIT_SUCCESS;
}
