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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <folly/AsyncSocket.h>
#include <folly/Range.h>
#include <folly/Synchronized.h>
#include <folly/io/async/AsyncTimeout.h>
#include <folly/io/async/EventBase.h>
#include <folly/logging/LogWriter.h>

namespace folly {

/**
 * A LogWriter implementation that sends log messages to a remote log collection
 * system over the network.
 *
 * This class uses folly::AsyncSocket to send log messages asynchronously to a
 * remote server. It supports both TCP and UDP protocols.
 */
class NetworkLogWriter : public LogWriter {
 public:
  enum class Protocol { TCP, UDP };

  /**
   * Create a NetworkLogWriter.
   *
   * @param host The hostname or IP address of the remote log server.
   * @param port The port number of the remote log server.
   * @param protocol The network protocol to use (TCP or UDP).
   * @param maxBufferSize The maximum size of the buffer for pending log messages.
   * @param reconnectInterval The interval (in milliseconds) to wait before
   *                          attempting to reconnect to the remote server.
   */
  NetworkLogWriter(
      std::string host,
      uint16_t port,
      Protocol protocol = Protocol::TCP,
      size_t maxBufferSize = 1024 * 1024,
      std::chrono::milliseconds reconnectInterval = std::chrono::seconds(5));

  ~NetworkLogWriter() override;

  void writeMessage(folly::StringPiece buffer, uint32_t flags = 0) override;
  void writeMessage(std::string&& buffer, uint32_t flags = 0) override;
  void flush() override;
  bool ttyOutput() const override { return false; }

 private:
  struct ConnectionState {
    std::shared_ptr<AsyncSocket> socket;
    std::vector<std::string> pendingMessages;
    size_t pendingBytes{0};
    bool connecting{false};
    bool closed{false};
  };

  class ReconnectTimeout : public AsyncTimeout {
   public:
    explicit ReconnectTimeout(NetworkLogWriter* writer)
        : AsyncTimeout(writer->eventBase_), writer_(writer) {}

    void timeoutExpired() noexcept override;

   private:
    NetworkLogWriter* writer_;
  };

  void connect();
  void onConnectSuccess();
  void onConnectError(const AsyncSocketException& ex);
  void onWriteSuccess();
  void onWriteError(const AsyncSocketException& ex);
  void onSocketClose();
  void sendPendingMessages();
  void scheduleReconnect();
  void cleanup();

  const std::string host_;
  const uint16_t port_;
  const Protocol protocol_;
  const size_t maxBufferSize_;
  const std::chrono::milliseconds reconnectInterval_;

  EventBase eventBase_;
  std::thread eventBaseThread_;
  Synchronized<ConnectionState> connectionState_;
  std::unique_ptr<ReconnectTimeout> reconnectTimeout_;
};
} // namespace folly