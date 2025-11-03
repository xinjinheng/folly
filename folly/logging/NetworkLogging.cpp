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

#include <folly/logging/LoggerDB.h>
#include <folly/logging/NetworkHandlerFactory.h>
#include <folly/logging/JSONLogFormatter.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogFormatterFactory.h>

namespace folly {

// Register the JSONLogFormatter
class JSONLogFormatterFactory : public LogFormatterFactory {
 public:
  std::string getType() const override { return "json"; }

  std::shared_ptr<LogFormatter> createFormatter(ConfigMap config) override {
    bool prettyPrint = false;
    auto prettyPrintIt = config.find("pretty");
    if (prettyPrintIt != config.end()) {
      prettyPrint = to<bool>(prettyPrintIt->second);
      config.erase(prettyPrintIt);
    }

    return std::make_shared<JSONLogFormatter>(prettyPrint);
  }
};

// Register the JSONLogFormatterFactory and NetworkHandlerFactory
static void registerNetworkLoggingFactories() {
  auto& db = LoggerDB::get();
  
  // Register JSONLogFormatterFactory
  db.registerHandlerFactory(
      std::make_unique<JSONLogFormatterFactory>(), true);
  
  // Register NetworkHandlerFactory
  db.registerHandlerFactory(
      std::make_unique<NetworkHandlerFactory>(), true);
}

// Initialize the network logging factories when the module is loaded
FOLLY_INITIALIZE(registerNetworkLoggingFactories);

} // namespace folly