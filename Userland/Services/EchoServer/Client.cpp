/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Client.h"
#include "LibCore/EventLoop.h"

Client::Client(int id, NonnullOwnPtr<Core::Stream::TCPSocket> socket)
    : m_id(id)
    , m_socket(move(socket))
{
    m_socket->on_ready_to_read = [this] {
        if (m_socket->is_eof())
            return;

        auto result = drain_socket();
        if (result.is_error()) {
            dbgln("Failed while trying to drain the socket: {}", result.error());
            Core::deferred_invoke([this, strong_this = NonnullRefPtr(*this)] { quit(); });
        }
    };
}

ErrorOr<void> Client::drain_socket()
{
    NonnullRefPtr<Client> protect(*this);

    auto maybe_buffer = ByteBuffer::create_uninitialized(1024);
    if (!maybe_buffer.has_value())
        return ENOMEM;
    auto buffer = maybe_buffer.release_value();

    while (TRY(m_socket->can_read_without_blocking())) {
        auto nread = TRY(m_socket->read(buffer));

        dbgln("Read {} bytes.", nread);

        if (m_socket->is_eof()) {
            Core::deferred_invoke([this, strong_this = NonnullRefPtr(*this)] { quit(); });
            break;
        }

        TRY(m_socket->write({ buffer.data(), nread }));
    }

    return {};
}

void Client::quit()
{
    m_socket->close();
    if (on_exit)
        on_exit();
}
