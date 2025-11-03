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

#include <folly/logging/LogHandlerFactory.h>
#include <folly/logging/NetworkLogWriter.h>

namespace folly {

/**
 * A LogHandlerFactory that creates NetworkLogHandler instances.
 *
 * This factory creates log handlers that send log messages to a remote
 * log collection system over the network.
 */
class NetworkHandlerFactory : public LogHandlerFactory {
 public:
  std::string getType() const override { return "network"; }

  std::shared_ptr<LogHandler> createHandler(ConfigMap config) override;

  void updateHandler(
      LogHandler* handler,
      const ConfigMap& config) override;

 private:
  NetworkLogWriter::Protocol parseProtocol(const std::string& protocolStr) const;
  std::chrono::milliseconds parseReconnectInterval(const std::string& intervalStr) const;
  size_t parseMaxBufferSize(const std::string& sizeStr) const;
};
} // namespace folly