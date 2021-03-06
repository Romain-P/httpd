//
// Created by romain on 3/1/19.
//

#include "Network.h"
#include "Server.h"

void Network::start() {
    info("tcp server listening on %s:%d", server.config().address().c_str(), server.config().port());

    _thread = boost::thread([this]() {
        boost::asio::socket_base::reuse_address option(true);
        _worker = std::make_unique<boost::asio::thread_pool>(server.config().poolSize());

        try {
            _acceptor = tcp::acceptor(_io, tcp::endpoint(tcp::v4(), server.config().port()));
            _acceptor.set_option(option);

            asyncAccept();

            _io.restart();
            _io.run();
        } catch (std::exception &e) {
            errors("tcp server error: %s", e.what());
        }
        info("tcp server stopped");
        _thread.detach();
    });
}

void Network::restart() {
    stop();
    start();
}

void Network::stop() {
    lock_t lock(_locker);

    for (auto &it: _sessions) {
        tcp::socket &socket = it.second->socket();

        if (socket.is_open()) {
            try {
                socket.shutdown(tcp::socket::shutdown_type::shutdown_both);
                socket.close();
            } catch (std::exception &e) {}
        }
    }

    _io.stop();
    _worker->stop();
    _sessions.clear();
    _sessionCounter = 0;

    info("tcp server cleared successfully");

    if (_thread.joinable())
        _thread.join();

    _acceptor.close();
}

void Network::delSession(ptr<Session> session, bool async) {
    if (async) {
        server.submit([this, session]() {
            delSession(session, false);
        });
        return;
    }

    lock_t lock(_locker);
    auto found = _sessions.find(session->id());

    if (session->socket().is_open()) {
        try {
            session->socket().shutdown(tcp::socket::shutdown_type::shutdown_both);
            session->socket().close();
        } catch (std::exception &e) {};
    }

    if (found != _sessions.end()) {
        _sessions.erase(session->id());
    }
}

void Network::asyncAccept() {
    lock_t lock(_locker);

    auto session = boost::make_shared<Session>(++_sessionCounter, _io);
    auto handler = boost::bind(&Network::onAccept, this, session, boost::asio::placeholders::error);

    _sessions[session->id()] = session;
    _acceptor.async_accept(session->socket(), handler);
}

void Network::onAccept(ptr<Session> session, const error_code &error) {
    if (_io.stopped()) return;

    if (!error) {
        server.submit([this, session]() {
            try {
                session->start();
            } catch (std::exception &e) {
                errors("session %zu stopped: %s", session->id(), e.what());
            }
            delSession(session, false);
        });
    }
    asyncAccept();
}

boost::thread &Network::thread() {
    return _thread;
}

boost::asio::thread_pool &Network::worker() {
    return *_worker.get();
}
