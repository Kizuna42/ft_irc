#!/usr/bin/env bash
# ft_irc Valgrind シナリオ（C: メモリリーク検証範囲拡張）
#
# 使い方:
#   chmod +x tests/irc_valgrind_scenario.sh
#   tests/irc_valgrind_scenario.sh [PORT] [PASSWORD]
#
# 例:
#   tests/irc_valgrind_scenario.sh 6668 password
#
# - 指定ポートで ircserv を Valgrind 経由で起動
# - Python テストスイート (tests/irc_basic_suite.py) を流す
# - サーバを停止し、Valgrind ログのサマリを表示

set -euo pipefail

PORT="${1:-6668}"
PASSWORD="${2:-password}"

LOG_FILE="valgrind_ft_irc_${PORT}.log"

echo "==> Starting ircserv under Valgrind on port ${PORT}"
echo "    log: ${LOG_FILE}"

valgrind --leak-check=full --show-leak-kinds=all \
  --log-file="${LOG_FILE}" \
  ./ircserv "${PORT}" "${PASSWORD}" &
VG_PID=$!

cleanup() {
  if kill -0 "${VG_PID}" 2>/dev/null; then
    echo "==> Stopping ircserv (pid=${VG_PID})"
    kill "${VG_PID}" 2>/dev/null || true
    wait "${VG_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

sleep 2

echo "==> Running Python test suite against Valgrind server"
IRC_HOST=127.0.0.1 IRC_PORT="${PORT}" IRC_PASSWORD="${PASSWORD}" \
  python3 tests/irc_basic_suite.py || true

sleep 2

cleanup

if [ -f "${LOG_FILE}" ]; then
  echo "==> Valgrind summary (tail)"
  tail -n 40 "${LOG_FILE}"
else
  echo "Valgrind log not found: ${LOG_FILE}"
fi

echo "==> Done."


