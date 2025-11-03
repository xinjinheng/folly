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

#include <folly/logging/JSONLogFormatter.h>

#include <ctime>

#include <folly/Format.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/portability/Time.h>

namespace folly {

JSONLogFormatter::JSONLogFormatter(bool prettyPrint) : prettyPrint_(prettyPrint) {
}

std::string JSONLogFormatter::formatMessage(
    const LogMessage& message, const LogCategory* handlerCategory) {
  std::string levelName = "UNKNOWN";
  auto level = message.getLevel();
  if (level < LogLevel::INFO) {
    levelName = "VERBOSE";
  } else if (level < LogLevel::WARN) {
    levelName = "INFO";
  } else if (level < LogLevel::ERR) {
    levelName = "WARN";
  } else if (level < LogLevel::CRITICAL) {
    levelName = "ERROR";
  } else if (level < LogLevel::DFATAL) {
    levelName = "CRITICAL";
  } else {
    levelName = "FATAL";
  }

  auto timestamp = formatTimestamp(message);
  auto escapedMessage = escapeJsonString(message.getMessage());
  auto escapedCategory = escapeJsonString(message.getCategory()->getName());
  auto escapedFile = escapeJsonString(message.getFile());
  auto escapedFunction = escapeJsonString(message.getFunction());

  if (prettyPrint_) {
    return folly::format(
        "{{\n"
        "  \"timestamp\": \"{}\",\n"
        "  \"level\": \"{}\",\n" 
        "  \"category\": \"{}\",\n"
        "  \"file\": \"{}\",\n"
        "  \"line\": {},\n"
        "  \"function\": \"{}\",\n"
        "  \"thread_id\": {},\n"
        "  \"message\": \"{}\"\n" 
        "}}\n",
        timestamp,
        levelName,
        escapedCategory,
        escapedFile,
        message.getLine(),
        escapedFunction,
        message.getThreadID(),
        escapedMessage)
        .str();
  } else {
    return folly::format(
        "{{\"timestamp\":\"{}\",\"level\":\"{}\",\"category\":\"{}\",\"file\":\"{}\",\"line\":{},\"function\":\"{}\",\"thread_id\":{},\"message\":\"{}\"}}\n",
        timestamp,
        levelName,
        escapedCategory,
        escapedFile,
        message.getLine(),
        escapedFunction,
        message.getThreadID(),
        escapedMessage)
        .str();
  }
}

std::string JSONLogFormatter::formatTimestamp(const LogMessage& message) const {
  auto time = message.getTimestamp();
  struct tm tm;
  localtime_r(&time.tv_sec, &tm);

  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
  return folly::format("{}.{:06d}", buf, static_cast<int>(time.tv_usec)).str();
}

std::string JSONLogFormatter::escapeJsonString(const std::string& str) const {
  std::string escaped;
  escaped.reserve(str.size() + 16);

  for (char c : str) {
    switch (c) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
          escaped += folly::format("\\u{:04x}", static_cast<unsigned int>(c)).str();
        } else {
          escaped += c;
        }
        break;
    }
  }

  return escaped;
}

} // namespace folly