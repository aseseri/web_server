#pragma once

// Standard includes
#include <fstream>
#include <iostream>

// Boost.Log includes
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/shared_ptr.hpp>

inline void init_logging() {
    namespace logging = boost::log;
    namespace sinks = boost::log::sinks;
    namespace keywords = boost::log::keywords;
    namespace expr = boost::log::expressions;

    logging::add_common_attributes();

    auto file_sink = logging::add_file_log(
        keywords::file_name = "logs/server_%Y-%m-%d_%H-%M-%S.%N.log",
        keywords::rotation_size = 10 * 1024 * 1024, // 10 MB
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), // Midnight
        keywords::format = (
            expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << "] [" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
                << "] [" << logging::trivial::severity
                << "] " << expr::smessage
        )
    );

    logging::add_console_log(std::clog,
        keywords::format = (
            expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S")
                << "] [" << logging::trivial::severity
                << "] " << expr::smessage
        )
    );

    logging::core::get()->set_filter(
        logging::trivial::severity >= logging::trivial::trace
    );

    // 🔥 Force auto-flush manually by setting backend
    file_sink->locked_backend()->auto_flush(true);

    BOOST_LOG_TRIVIAL(info) << "Logging initialized";
}

inline void shutdown_logging() {
    boost::log::core::get()->flush();
}
