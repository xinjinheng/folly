/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/logging/NetworkLogWriter.h>

#include <chrono>
#include <thread>

#include <folly/Exception.h>
#include <folly/Format.h>
#include <folly/io/async/AsyncSocketException.h>
#include <folly/logging/LoggerDB.h>

using namespace std::chrono_literals;

namespace folly {

NetworkLogWriter::NetworkLogWriter(
    std::string host,
    uint16_t port,
    Protocol protocol,
    size_t maxBufferSize,
    std::chrono::milliseconds reconnectInterval)
    : host_(std::move(host)),
      port_(port),
      protocol_(protocol),
      maxBufferSize_(maxBufferSize),
      reconnectInterval_(reconnectInterval),
      reconnectTimeout_(std::make_unique<ReconnectTimeout>(this)) {
  // Start the event base thread
  eventBaseThread_ = std::thread([this] { eventBase_.loopForever(); });

  // Connect to the remote server
  eventBase_.runInEventBaseThread([this] { connect(); });
}

NetworkLogWriter::~NetworkLogWriter() {
  cleanup();
}

void NetworkLogWriter::writeMessage(StringPiece buffer, uint32_t flags) {
  writeMessage(buffer.str(), flags);
}

void NetworkLogWriter::writeMessage(std::string&& buffer, uint32_t flags) {
  // If the message should never be discarded, we need to handle it specially
  if (flags & LogWriter::NEVER_DISCARD) {
    // TODO: Implement special handling for never discard messages
    // For now, just treat it as a normal message
  }

  auto state = connectionState_.lock();

  // Check if we need to discard the message
  if (state->pendingBytes + buffer.size() > maxBufferSize_ &&
      !(flags & LogWriter::NEVER_DISCARD)) {
    // TODO: Implement discard callback
    return;
  }

  // Add the message to the pending queue
  state->pendingMessages.emplace_back(std::move(buffer));
  state->pendingBytes += state->pendingMessages.back().size();

  // If we're connected, send the pending messages
  if (state->socket && state->socket->good()) {
    eventBase_.runInEventBaseThread([this] { sendPendingMessages(); });
  }
}

void NetworkLogWriter::flush() {
  // TODO: Implement flush functionality
  // For now, just send any pending messages
  eventBase_.runInEventBaseThread([this] { sendPendingMessages(); });
}

void NetworkLogWriter::connect() {
  auto state = connectionState_.lock();

  if (state->connecting || state->socket && state->socket->good()) {
    return;
  }

  state->connecting = true;

  try {
    std::shared_ptr<AsyncSocket> socket;
    if (protocol_ == Protocol::TCP) {
      socket = std::make_shared<AsyncSocket>(&eventBase_);
    } else {
      // TODO: Implement UDP support
      throw std::runtime_error("UDP protocol not yet supported");
    }

    socket->connect(
        this,
        host_,
        port_,
        std::chrono::milliseconds(5000)); // 5 second timeout

    state->socket = socket;
  } catch (const std::exception& ex) {
    LOG(ERROR) << "Failed to create socket for network log writer: " << ex.what();
    state->connecting = false;
    scheduleReconnect();
  }
}

void NetworkLogWriter::onConnectSuccess() {
  auto state = connectionState_.lock();
  state->connecting = false;

  LOG(INFO) << "Successfully connected to network log server at " << host_ << ":" << port_;

  // Send any pending messages
  if (!state->pendingMessages.empty()) {
    sendPendingMessages();
  }
}

void NetworkLogWriter::onConnectError(const AsyncSocketException& ex) {
  auto state = connectionState_.lock();
  state->connecting = false;
  state->socket.reset();

  LOG(ERROR) << "Failed to connect to network log server at " << host_ << ":" << port_ << ": " << ex.what();

  scheduleReconnect();
}

void NetworkLogWriter::onWriteSuccess() {
  auto state = connectionState_.lock();

  // Remove the first message from the pending queue
  if (!state->pendingMessages.empty()) {
    state->pendingBytes -= state->pendingMessages.front().size();
    state->pendingMessages.erase(state->pendingMessages.begin());
  }

  // Send the next message if there are more pending
  if (!state->pendingMessages.empty() && state->socket && state->socket->good()) {
    sendPendingMessages();
  }
}

void NetworkLogWriter::onWriteError(const AsyncSocketException& ex) {
  auto state = connectionState_.lock();

  LOG(ERROR) << "Failed to write to network log server: " << ex.what();

  // If the socket is still good, try to send the message again
  if (state->socket && state->socket->good()) {
    sendPendingMessages();
  } else {
    // Otherwise, schedule a reconnect
    state->socket.reset();
    scheduleReconnect();
  }
}

void NetworkLogWriter::onSocketClose() {
  auto state = connectionState_.lock();
  state->socket.reset();

  LOG(INFO) << "Connection to network log server closed";

  scheduleReconnect();
}

void NetworkLogWriter::sendPendingMessages() {
  auto state = connectionState_.lock();

  if (state->pendingMessages.empty() || !state->socket || !state->socket->good()) {
    return;
  }

  // Send the first pending message
  const auto& message = state->pendingMessages.front();
  state->socket->write(this, message.data(), message.size());
}

void NetworkLogWriter::scheduleReconnect() {
  if (!reconnectTimeout_->isScheduled()) {
    reconnectTimeout_->scheduleTimeout(reconnectInterval_);
  }
}

void NetworkLogWriter::cleanup() {
  eventBase_.runInEventBaseThread([this] { 
    reconnectTimeout_->cancelTimeout();
    eventBase_.terminateLoopSoon();
  });

  if (eventBaseThread_.joinable()) {
    eventBaseThread_.join();
  }

  auto state = connectionState_.lock();
  state->closed = true;
  state->socket.reset();
}

void NetworkLogWriter::ReconnectTimeout::timeoutExpired() noexcept {
  try {
    writer_->connect();
  } catch (const std::exception& ex) {
    LOG(ERROR) << "Failed to reconnect to network log server: " << ex.what();
    writer_->scheduleReconnect();
  }
}

} // namespace folly