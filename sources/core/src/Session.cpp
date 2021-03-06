//
// Created by romain on 2/28/19.
//

#include "Session.h"
#include <boost/bind.hpp>
#include "Server.h"
#include "Http.h"

#define HEADERS_LIMIT   16000

void Session::start() {
    _context = server.modules().newModuleContext();

    connectionStart();
    readRequest();
    createResponse();
    sendResponse();
    connectionEnd();
}

void Session::connectionStart() {
    auto result = pipeline([this](RequestHandler::pointer handler) {
        return handler->onConnectionStart(server.connectionInfos(), _socket);
    });

    assertTrue(result != http::code::internal_error, "error encountered when connection started");
}

void Session::connectionEnd() {
    pipeline([this](RequestHandler::pointer handler) {
        return handler->onConnectionEnd(server.connectionInfos(), _socket);
    });
}

void Session::connectionRead() {
    std::vector<char> buffer;
    sizet readSize = 0;

    buffer.resize(server.config().networkReadBuffer());

    auto result = pipeline([this, &buffer, &readSize](RequestHandler::pointer handler) {
        return handler->onConnectionRead(server.connectionInfos(), _socket, buffer, readSize);
    });

    if (result != http::code::internal_error) {
        assertTrue(readSize > 0, "closed by peer");
        _readBuffer.insert(_readBuffer.end(), buffer.begin(), buffer.begin() + readSize);
    }

    assertTrue(result != http::code::internal_error, "no suitable module found for read sockets of incoming connections");
}

void Session::readRequest() {
    std::pair<headers_end_offset, body_start_offset> offsets;

    do {
        connectionRead();
        offsets = http::request::parser::requestOffsets(_readBuffer);
        assertTrue(_readBuffer.size() < HEADERS_LIMIT, "invalid header");
    } while (offsets == http::request::parser::offsets_not_found);

    assertTrue(http::request::parser::parseRequest(_readBuffer, sizet(offsets.first), _request), "invalid request received");

    auto entry = _request.headers.find(http::headers::content_length);
    if (entry == _request.headers.end()) return;

    ssizet bodyLength = std::stoi(entry->second);
    if (bodyLength <= 0) return;

    sizet totalSize = sizet(offsets.second) + bodyLength;
    while (_readBuffer.size() < totalSize)
        connectionRead();

    _request.body.insert(_request.body.end(), _readBuffer.begin() + offsets.second, _readBuffer.end());
}

void Session::createResponse() {
    auto result = pipeline([this](RequestHandler::pointer handler) {
        return handler->onBeforeRequest(server.connectionInfos(), _request);
    });

    if (http::is_error_code(result)) {
        result = pipeline([this, &result](RequestHandler::pointer handler) {
            return handler->onRequestError(server.connectionInfos(), result, _response);
        });
    }
    else {
        result = pipeline([this](RequestHandler::pointer handler) {
            return handler->onRequest(server.connectionInfos(), _request, _response);
        });
    }

    if (result == http::code::internal_error)
        _response = http::responses::internal_error;
}

void Session::sendResponse() {
    pipeline([this](RequestHandler::pointer handler) {
        return handler->onResponse(server.connectionInfos(), _response);
    });

    http::responses::serializer::serialize(_response, _writeBuffer);
    connectionWrite();
}

void Session::connectionWrite() {
    sizet unused = _writeBuffer.size();

    auto result = pipeline([this, &unused](RequestHandler::pointer handler) {
        return handler->onConnectionWrite(server.connectionInfos(), _socket, _writeBuffer, unused);
    });

    assertTrue(result != http::code::internal_error, "no suitable module found for write on sockets");
}

HookResultType Session::pipeline(std::function<HookResultType(RequestHandler::pointer)> const &hook) {
    return server.modules().executePipeline(_context, hook);
}

tcp::socket &Session::socket() {
    return _socket;
}

sizet Session::id() const {
    return _id;
}