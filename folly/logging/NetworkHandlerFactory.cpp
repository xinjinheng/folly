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

#include <folly/logging/NetworkHandlerFactory.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

#include <folly/Conv.h>
#include <folly/Format.h>
#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>

namespace folly {

std::shared_ptr<LogHandler> NetworkHandlerFactory::createHandler(ConfigMap config) {
  // Get required parameters
  auto hostIt = config.find("host");
  if (hostIt == config.end()) {
    throw std::invalid_argument("network handler requires 'host' parameter");
  }
  std::string host = hostIt->second;
  config.erase(hostIt);

  auto portIt = config.find("port");
  if (portIt == config.end()) {
    throw std::invalid_argument("network handler requires 'port' parameter");
  }
  uint16_t port = to<uint16_t>(portIt->second);
  config.erase(portIt);

  // Get optional parameters
  NetworkLogWriter::Protocol protocol = NetworkLogWriter::Protocol::TCP;
  auto protocolIt = config.find("protocol");
  if (protocolIt != config.end()) {
    protocol = parseProtocol(protocolIt->second);
    config.erase(protocolIt);
  }

  size_t maxBufferSize = 1024 * 1024; // 1MB
  auto maxBufferSizeIt = config.find("max_buffer_size");
  if (maxBufferSizeIt != config.end()) {
    maxBufferSize = parseMaxBufferSize(maxBufferSizeIt->second);
    config.erase(maxBufferSizeIt);
  }

  std::chrono::milliseconds reconnectInterval = std::chrono::seconds(5);
  auto reconnectIntervalIt = config.find("reconnect_interval");
  if (reconnectIntervalIt != config.end()) {
    reconnectInterval = parseReconnectInterval(reconnectIntervalIt->second);
    config.erase(reconnectIntervalIt);
  }

  // Create the NetworkLogWriter
  auto writer = std::make_shared<NetworkLogWriter>(
      host, port, protocol, maxBufferSize, reconnectInterval);

  // Create a StandardLogHandler with this writer
  StandardLogHandlerFactory factory;
  auto handler = factory.createHandler(config);

  // Replace the writer in the StandardLogHandler with our NetworkLogWriter
  auto standardHandler = dynamic_cast<StandardLogHandler*>(handler.get());
  if (!standardHandler) {
    throw std::runtime_error("expected StandardLogHandler");
  }
  standardHandler->setWriter(std::move(writer));

  return handler;
}

void NetworkHandlerFactory::updateHandler(
    LogHandler* handler,
    const ConfigMap& config) {
  // TODO: Implement updateHandler functionality
  // For now, just throw an exception since we don't support updating yet
  throw std::runtime_error("NetworkHandlerFactory::updateHandler not implemented");
}

NetworkLogWriter::Protocol NetworkHandlerFactory::parseProtocol(const std::string& protocolStr) const {
  std::string protocolLower = toLowerAscii(protocolStr);
  if (protocolLower == "tcp") {
    return NetworkLogWriter::Protocol::TCP;
  } else if (protocolLower == "udp") {
    return NetworkLogWriter::Protocol::UDP;
  } else {
    throw std::invalid_argument(folly::sformat("unknown protocol '{}', expected 'tcp' or 'udp'", protocolStr));
  }
}

std::chrono::milliseconds NetworkHandlerFactory::parseReconnectInterval(const std::string& intervalStr) const {
  // Parse a time interval string like "5s" or "1000ms"
  size_t pos = 0;
  while (pos < intervalStr.size() && std::isdigit(intervalStr[pos])) {
    ++pos;
  }

  if (pos == 0 || pos == intervalStr.size()) {
    throw std::invalid_argument(folly::sformat("invalid reconnect interval '{}'", intervalStr));
  }

  std::string numberStr = intervalStr.substr(0, pos);
  std::string unitStr = intervalStr.substr(pos);

  uint64_t number = to<uint64_t>(numberStr);

  if (unitStr == "ms") {
    return std::chrono::milliseconds(number);
  } else if (unitStr == "s") {
    return std::chrono::seconds(number);
  } else if (unitStr == "m") {
    return std::chrono::minutes(number);
  } else if (unitStr == "h") {
    return std::chrono::hours(number);
  } else {
    throw std::invalid_argument(folly::sformat("invalid time unit '{}' in reconnect interval", unitStr));
  }
}

size_t NetworkHandlerFactory::parseMaxBufferSize(const std::string& sizeStr) const {
  // Parse a size string like "1MB" or "1024KB"
  size_t pos = 0;
  while (pos < sizeStr.size() && std::isdigit(sizeStr[pos])) {
    ++pos;
  }

  if (pos == 0 || pos == sizeStr.size()) {
    throw std::invalid_argument(folly::sformat("invalid max buffer size '{}'", sizeStr));
  }

  std::string numberStr = sizeStr.substr(0, pos);
  std::string unitStr = sizeStr.substr(pos);

  uint64_t number = to<uint64_t>(numberStr);
  size_t multiplier = 1;

  if (unitStr == "B") {
    multiplier = 1;
  } else if (unitStr == "KB") {
    multiplier = 1024;
  } else if (unitStr == "MB") {
    multiplier = 1024 * 1024;
  } else if (unitStr == "GB") {
    multiplier = 1024 * 1024 * 1024;
  } else {
    throw std::invalid_argument(folly::sformat("invalid size unit '{}' in max buffer size", unitStr));
  }

  return number * multiplier;
}

} // namespace folly