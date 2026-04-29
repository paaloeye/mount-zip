// Copyright 2024 Google LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "log.h"

#include <unistd.h>

#include <cstring>

LogLevel g_log_level = LogLevel::INFO;

bool g_latest_log_is_ephemeral = false;

void SetLogLevel(LogLevel const level) {
  g_log_level = level;
  setlogmask(LOG_UPTO(static_cast<int>(level)));
}

Logger::~Logger() {
  if (err_ >= 0) {
    if (LOG_IS_ON(DEBUG)) {
      oss_ << ": Error " << err_;
    }
    oss_ << ": " << strerror(err_);
  }

  if (g_latest_log_is_ephemeral && isatty(STDERR_FILENO)) {
    std::string_view const s = "\033[F\033[K";
    std::ignore = write(STDERR_FILENO, s.data(), s.size());
  }

  syslog(static_cast<int>(level_), "%s", std::move(oss_).str().c_str());
  g_latest_log_is_ephemeral = ephemeral_;
}
