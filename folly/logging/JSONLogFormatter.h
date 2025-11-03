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

#include <string>

#include <folly/Range.h>
#include <folly/logging/LogFormatter.h>

namespace folly {

/**
 * A LogFormatter implementation that produces messages in JSON format.
 *
 * This formatter outputs log messages as JSON objects with the following fields:
 * - timestamp: The time the log message was created (ISO 8601 format)
 * - level: The log level (e.g., "INFO", "WARN", "ERROR")
 * - category: The log category (e.g., "folly.example")
 * - file: The file where the log message was created
 * - line: The line number where the log message was created
 * - function: The function where the log message was created
 * - thread_id: The ID of the thread that created the log message
 * - message: The log message content
 */
class JSONLogFormatter : public LogFormatter {
 public:
  explicit JSONLogFormatter(bool prettyPrint = false);
  std::string formatMessage(
      const LogMessage& message, const LogCategory* handlerCategory) override;

 private:
  std::string formatTimestamp(const LogMessage& message) const;
  std::string escapeJsonString(const std::string& str) const;

  const bool prettyPrint_;
};
} // namespace folly