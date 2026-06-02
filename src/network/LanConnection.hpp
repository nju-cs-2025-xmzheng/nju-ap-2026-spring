#pragma once

#include "common/__cpo.hpp"
#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace Synera::network {

namespace __tag {
struct start_host_t {};
struct join_host_t {};
struct poll_event_t {};
struct send_text_t {};
struct is_connected_t {};
struct shutdown_t {};
} // namespace __tag

namespace __fn {
struct start_host_fn {
    template <typename C>
    auto operator()(C &&conn, std::uint16_t port) const
        noexcept(noexcept(tag_invoke(__tag::start_host_t{},
                                     std::forward<C>(conn), port)))
            -> decltype(auto) {
        return tag_invoke(__tag::start_host_t{}, std::forward<C>(conn), port);
    }
};

struct join_host_fn {
    template <typename C>
    auto operator()(C &&conn, const std::string &host, std::uint16_t port) const
        noexcept(noexcept(tag_invoke(__tag::join_host_t{},
                                     std::forward<C>(conn), host, port)))
            -> decltype(auto) {
        return tag_invoke(__tag::join_host_t{}, std::forward<C>(conn), host,
                          port);
    }
};

struct poll_event_fn {
    template <typename C>
    auto operator()(C &&conn) const
        noexcept(noexcept(tag_invoke(__tag::poll_event_t{},
                                     std::forward<C>(conn))))
            -> decltype(auto) {
        return tag_invoke(__tag::poll_event_t{}, std::forward<C>(conn));
    }
};

struct send_text_fn {
    template <typename C>
    auto operator()(C &&conn, std::string text) const
        noexcept(noexcept(tag_invoke(__tag::send_text_t{},
                                     std::forward<C>(conn), std::move(text))))
            -> decltype(auto) {
        return tag_invoke(__tag::send_text_t{}, std::forward<C>(conn),
                          std::move(text));
    }
};

struct is_connected_fn {
    template <typename C>
    auto operator()(C &&conn) const
        noexcept(noexcept(tag_invoke(__tag::is_connected_t{},
                                     std::forward<C>(conn))))
            -> decltype(auto) {
        return tag_invoke(__tag::is_connected_t{}, std::forward<C>(conn));
    }
};

struct shutdown_fn {
    template <typename C>
    auto operator()(C &&conn) const
        noexcept(noexcept(tag_invoke(__tag::shutdown_t{},
                                     std::forward<C>(conn))))
            -> decltype(auto) {
        return tag_invoke(__tag::shutdown_t{}, std::forward<C>(conn));
    }
};
} // namespace __fn

inline constexpr __fn::start_host_fn start_host{};
inline constexpr __fn::join_host_fn join_host{};
inline constexpr __fn::poll_event_fn poll_event{};
inline constexpr __fn::send_text_fn send_text{};
inline constexpr __fn::is_connected_fn is_connected{};
inline constexpr __fn::shutdown_fn shutdown{};

enum class EventType { Connected, Disconnected, Message, Error };

struct Event {
    EventType type = EventType::Message;
    std::string text;
};

class LanConnection {
  public:
    LanConnection() : socket_(io_), acceptor_(io_) {}

    ~LanConnection() { shutdown(*this); }

    LanConnection(const LanConnection &) = delete;
    LanConnection &operator=(const LanConnection &) = delete;

    friend bool tag_invoke(__tag::start_host_t, LanConnection &conn,
                           std::uint16_t port) {
        conn.reset();
        boost::system::error_code ec;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(),
                                                port);
        conn.acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return conn.fail("open: " + ec.message());
        conn.acceptor_.set_option(
            boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
        if (ec)
            return conn.fail("reuse_address: " + ec.message());
        conn.acceptor_.bind(endpoint, ec);
        if (ec)
            return conn.fail("bind: " + ec.message());
        conn.acceptor_.listen(boost::asio::socket_base::max_listen_connections,
                              ec);
        if (ec)
            return conn.fail("listen: " + ec.message());

        conn.acceptor_.async_accept(
            conn.socket_, [&conn](boost::system::error_code accept_ec) {
                if (accept_ec) {
                    conn.fail("accept: " + accept_ec.message());
                    return;
                }
                conn.connected_ = true;
                conn.push_event({EventType::Connected, ""});
                conn.read_header();
            });
        conn.run();
        return true;
    }

    friend bool tag_invoke(__tag::join_host_t, LanConnection &conn,
                           const std::string &host, std::uint16_t port) {
        conn.reset();
        auto resolver =
            std::make_shared<boost::asio::ip::tcp::resolver>(conn.io_);
        resolver->async_resolve(
            host, std::to_string(port),
            [&conn,
             resolver](boost::system::error_code resolve_ec,
                       boost::asio::ip::tcp::resolver::results_type endpoints) {
                if (resolve_ec) {
                    conn.fail("resolve: " + resolve_ec.message());
                    return;
                }
                boost::asio::async_connect(
                    conn.socket_, endpoints,
                    [&conn](boost::system::error_code connect_ec,
                            const boost::asio::ip::tcp::endpoint &) {
                        if (connect_ec) {
                            conn.fail("connect: " + connect_ec.message());
                            return;
                        }
                        conn.connected_ = true;
                        conn.push_event({EventType::Connected, ""});
                        conn.read_header();
                    });
            });
        conn.run();
        return true;
    }

    friend std::optional<Event> tag_invoke(__tag::poll_event_t,
                                           LanConnection &conn) {
        std::scoped_lock lock(conn.mutex_);
        if (conn.events_.empty()) {
            return std::nullopt;
        }
        auto event = std::move(conn.events_.front());
        conn.events_.pop_front();
        return event;
    }

    friend bool tag_invoke(__tag::send_text_t, LanConnection &conn,
                           std::string text) {
        if (!conn.connected_.load()) {
            return false;
        }
        conn.post_write(std::move(text));
        return true;
    }

    friend bool tag_invoke(__tag::is_connected_t, const LanConnection &conn) {
        return conn.connected_.load();
    }

    friend void tag_invoke(__tag::shutdown_t, LanConnection &conn) {
        conn.connected_ = false;
        boost::system::error_code ec;
        conn.socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        conn.socket_.close(ec);
        conn.acceptor_.close(ec);
        conn.io_.stop();
        if (conn.thread_.joinable()) {
            conn.thread_.join();
        }
    }

  private:
    void reset() {
        shutdown(*this);
        io_.restart();
        socket_ = boost::asio::ip::tcp::socket(io_);
        acceptor_ = boost::asio::ip::tcp::acceptor(io_);
        {
            std::scoped_lock lock(mutex_);
            events_.clear();
            write_queue_.clear();
        }
        connected_ = false;
    }

    void run() {
        thread_ = std::thread([this] { io_.run(); });
    }

    bool fail(std::string message) {
        connected_ = false;
        push_event({EventType::Error, std::move(message)});
        return false;
    }

    void push_event(Event event) {
        std::scoped_lock lock(mutex_);
        events_.push_back(std::move(event));
    }

    void read_header() {
        boost::asio::async_read(
            socket_, boost::asio::buffer(read_header_),
            [this](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec);
                    return;
                }
                std::uint32_t size = (std::uint32_t(read_header_[0]) << 24) |
                                     (std::uint32_t(read_header_[1]) << 16) |
                                     (std::uint32_t(read_header_[2]) << 8) |
                                     std::uint32_t(read_header_[3]);
                if (size == 0 || size > max_message_size_) {
                    fail("invalid message size");
                    return;
                }
                read_body_.assign(size, '\0');
                read_body();
            });
    }

    void read_body() {
        boost::asio::async_read(
            socket_, boost::asio::buffer(read_body_),
            [this](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec);
                    return;
                }
                push_event({EventType::Message, read_body_});
                read_header();
            });
    }

    void post_write(std::string text) {
        boost::asio::post(io_, [this, text = std::move(text)]() mutable {
            bool idle = write_queue_.empty();
            write_queue_.push_back(frame(std::move(text)));
            if (idle) {
                write_next();
            }
        });
    }

    static std::string frame(std::string text) {
        std::uint32_t size = static_cast<std::uint32_t>(text.size());
        std::string framed(4, '\0');
        framed[0] = static_cast<char>((size >> 24) & 0xff);
        framed[1] = static_cast<char>((size >> 16) & 0xff);
        framed[2] = static_cast<char>((size >> 8) & 0xff);
        framed[3] = static_cast<char>(size & 0xff);
        framed += text;
        return framed;
    }

    void write_next() {
        boost::asio::async_write(
            socket_, boost::asio::buffer(write_queue_.front()),
            [this](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    handle_disconnect(ec);
                    return;
                }
                write_queue_.pop_front();
                if (!write_queue_.empty()) {
                    write_next();
                }
            });
    }

    void handle_disconnect(const boost::system::error_code &ec) {
        if (!connected_.load()) {
            return;
        }
        connected_ = false;
        push_event({EventType::Disconnected, ec.message()});
    }

  private:
    static constexpr std::uint32_t max_message_size_ = 1024 * 1024;

    boost::asio::io_context io_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::thread thread_;
    std::mutex mutex_;
    std::deque<Event> events_;
    std::deque<std::string> write_queue_;
    std::array<unsigned char, 4> read_header_{};
    std::string read_body_;
    std::atomic_bool connected_ = false;
};

} // namespace Synera::network
