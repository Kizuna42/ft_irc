#!/usr/bin/env python3
"""
ft_irc 自動テストスイート（A: テストカバレッジ拡張）

- 認証 / ニックネーム重複
- チャンネル / PRIVMSG / NOTICE
- モード i/t/k/l/o + KICK/INVITE/TOPIC
- 部分コマンド / QUIT

事前に IRC サーバを起動しておくこと:
    ./ircserv 6667 password

環境変数で接続先を変更可能:
    IRC_HOST / IRC_PORT / IRC_PASSWORD
"""

import errno
import os
import socket
import sys
import time
from typing import List, Tuple


HOST = os.getenv("IRC_HOST", "127.0.0.1")
PORT = int(os.getenv("IRC_PORT", "6667"))
PASSWORD = os.getenv("IRC_PASSWORD", "password")


class IRCClient:
    def __init__(self, nick: str, username: str = None, realname: str = None):
        self.nick = nick
        self.username = username or nick
        self.realname = realname or nick
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((HOST, PORT))
        # 受信は非ブロッキングで行う（サーバは poll() ベース）
        self.sock.setblocking(False)

    def send_raw(self, data: str) -> None:
        """CRLF 付与なしで生データ送信."""
        self.sock.sendall(data.encode("utf-8"))

    def send_cmd(self, line: str) -> None:
        """1 コマンド行を送信（CRLF 付与）。"""
        self.send_raw(line + "\r\n")

    def recv_all(self, overall_timeout: float = 1.0, idle_timeout: float = 0.2) -> str:
        """一定時間データが届かなくなるまで読み取る簡易 recv."""
        data = b""
        start = time.time()
        last_data = start
        while True:
            now = time.time()
            if now - start > overall_timeout:
                break
            if now - last_data > idle_timeout:
                break
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                data += chunk
                last_data = time.time()
            except OSError as e:
                if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                    time.sleep(0.02)
                    continue
                else:
                    break
        return data.decode("utf-8", "ignore")

    def close(self) -> None:
        try:
            self.sock.close()
        except Exception:
            pass


def new_authed_client(nick: str) -> Tuple[IRCClient, str]:
    """PASS/NICK/USER まで済ませたクライアントを生成。"""
    c = IRCClient(nick)
    c.send_cmd(f"PASS {PASSWORD}")
    c.send_cmd(f"NICK {nick}")
    c.send_cmd(f"USER {nick} 0 * :{nick} User")
    resp = c.recv_all()
    return c, resp


Result = Tuple[str, bool, str]


def record(results: List[Result], name: str, passed: bool, info: str) -> None:
    results.append((name, passed, info))


# テスト本体 --------------------------------------------------------------

def run_tests() -> List[Result]:
    results: List[Result] = []

    # T01: 認証成功
    try:
        c, resp = new_authed_client("alice1")
        ok = (" 001 " in resp) or ("Welcome" in resp)
        record(results, "basic_auth_success", ok, resp)
        c.close()
    except Exception as e:
        record(results, "basic_auth_success", False, f"Exception: {e!r}")

    # T02: 誤パスワード
    try:
        c = IRCClient("alice2")
        c.send_cmd("PASS wrongpassword")
        c.send_cmd("NICK alice2")
        c.send_cmd("USER alice2 0 * :Alice2 User")
        resp = c.recv_all()
        ok = (" 464 " in resp) or ("Password incorrect" in resp)
        record(results, "auth_wrong_password", ok, resp)
        c.close()
    except Exception as e:
        record(results, "auth_wrong_password", False, f"Exception: {e!r}")

    # T03: PASS なしで JOIN 試行
    try:
        c = IRCClient("alice3")
        c.send_cmd("NICK alice3")
        c.send_cmd("USER alice3 0 * :Alice3 User")
        c.send_cmd("JOIN #test")
        resp = c.recv_all()
        ok = (" 451 " in resp) or ("You have not registered" in resp)
        record(results, "auth_missing_pass", ok, resp)
        c.close()
    except Exception as e:
        record(results, "auth_missing_pass", False, f"Exception: {e!r}")

    # T04: ニックネーム重複
    try:
        c1, _ = new_authed_client("dupnick")
        c2 = IRCClient("dupnick")
        c2.send_cmd(f"PASS {PASSWORD}")
        c2.send_cmd("NICK dupnick")
        c2.send_cmd("USER dupnick2 0 * :Dup User2")
        resp2 = c2.recv_all()
        ok = (" 433 " in resp2) or ("Nickname is already in use" in resp2)
        record(results, "nick_collision", ok, resp2)
        c1.close()
        c2.close()
    except Exception as e:
        record(results, "nick_collision", False, f"Exception: {e!r}")

    # T05: チャンネル PRIVMSG ブロードキャスト
    try:
        a, _ = new_authed_client("alice4")
        b, _ = new_authed_client("bob4")

        a.send_cmd("JOIN #chan1")
        _ = a.recv_all()
        b.send_cmd("JOIN #chan1")
        _ = b.recv_all()

        a.send_cmd("PRIVMSG #chan1 :Hello Bob")
        time.sleep(0.2)
        rb = b.recv_all()
        ok = "PRIVMSG #chan1 :Hello Bob" in rb and "alice4" in rb
        ra = a.recv_all()
        info = "Bob_recv=" + rb.replace("\n", "\\n") + "\nAlice_recv=" + ra.replace("\n", "\\n")
        record(results, "channel_privmsg_broadcast", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "channel_privmsg_broadcast", False, f"Exception: {e!r}")

    # T06: ユーザー宛て PRIVMSG
    try:
        a, _ = new_authed_client("alice_u1")
        b, _ = new_authed_client("bob_u1")

        a.send_cmd("PRIVMSG bob_u1 :Hello user")
        time.sleep(0.2)
        rb = b.recv_all()
        ok = "PRIVMSG bob_u1 :Hello user" in rb and "alice_u1" in rb
        ra = a.recv_all()
        info = "Bob_recv=" + rb.replace("\n", "\\n") + "\nAlice_recv=" + ra.replace("\n", "\\n")
        record(results, "user_privmsg_basic", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "user_privmsg_basic", False, f"Exception: {e!r}")

    # T07: ユーザー宛て NOTICE
    try:
        a, _ = new_authed_client("alice_n1")
        b, _ = new_authed_client("bob_n1")

        a.send_cmd("NOTICE bob_n1 :Hello notice")
        time.sleep(0.2)
        rb = b.recv_all()
        ok = "NOTICE bob_n1 :Hello notice" in rb and "alice_n1" in rb
        ra = a.recv_all()
        # NOTICE はエラー・応答を返さないことが仕様なので、送信側には何も来ないケースが多い
        info = "Bob_recv=" + rb.replace("\n", "\\n") + "\nAlice_recv=" + ra.replace("\n", "\\n")
        record(results, "user_notice_basic", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "user_notice_basic", False, f"Exception: {e!r}")

    # T08: NOTICE パラメータ不足でもエラーを返さない
    try:
        a, _ = new_authed_client("alice_n2")
        # 受信バッファを空にする
        _ = a.recv_all()
        a.send_cmd("NOTICE")
        time.sleep(0.2)
        ra = a.recv_all()
        ok = ra == ""
        record(results, "notice_missing_params_no_error", ok, ra.replace("\n", "\\n"))
        a.close()
    except Exception as e:
        record(results, "notice_missing_params_no_error", False, f"Exception: {e!r}")

    # T09: 招待制チャンネル +i / INVITE
    try:
        a, _ = new_authed_client("alice5")
        b, _ = new_authed_client("bob5")

        a.send_cmd("JOIN #private1")
        _ = a.recv_all()
        a.send_cmd("MODE #private1 +i")
        _ = a.recv_all()

        b.send_cmd("JOIN #private1")
        resp_fail = b.recv_all()
        cond_err = (" 473 " in resp_fail) or ("Cannot join channel (+i)" in resp_fail)

        a.send_cmd("INVITE bob5 #private1")
        _ = a.recv_all()
        b.send_cmd("JOIN #private1")
        resp_ok = b.recv_all()
        cond_ok = ("JOIN #private1" in resp_ok) or (" 331 " in resp_ok) or (" 353 " in resp_ok)

        ok = cond_err and cond_ok
        info = "join_fail=" + resp_fail.replace("\n", "\\n") + "\njoin_ok=" + resp_ok.replace("\n", "\\n")
        record(results, "mode_i_invite_only", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "mode_i_invite_only", False, f"Exception: {e!r}")

    # T10: 非オペレーター KICK 拒否
    try:
        a, _ = new_authed_client("alice6")
        b, _ = new_authed_client("bob6")

        a.send_cmd("JOIN #kicktest")
        _ = a.recv_all()
        b.send_cmd("JOIN #kicktest")
        _ = b.recv_all()

        b.send_cmd("KICK #kicktest alice6 :Test")
        resp = b.recv_all()
        ok = (" 482 " in resp) or ("You're not channel operator" in resp)
        record(results, "kick_non_operator_forbidden", ok, resp.replace("\n", "\\n"))

        a.close()
        b.close()
    except Exception as e:
        record(results, "kick_non_operator_forbidden", False, f"Exception: {e!r}")

    # T11: オペレーター KICK 成功
    try:
        a, _ = new_authed_client("alice7")
        b, _ = new_authed_client("bob7")

        a.send_cmd("JOIN #kickok")
        _ = a.recv_all()
        b.send_cmd("JOIN #kickok")
        _ = b.recv_all()

        a.send_cmd("KICK #kickok bob7 :Bye")
        ra = a.recv_all()
        rb = b.recv_all()

        cond_a = "KICK #kickok bob7" in ra
        cond_b = ("KICK #kickok bob7" in rb) or rb == ""
        ok = cond_a and cond_b
        info = "alice_view=" + ra.replace("\n", "\\n") + "\nbob_view=" + rb.replace("\n", "\\n")
        record(results, "kick_operator_success", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "kick_operator_success", False, f"Exception: {e!r}")

    # T12: MODE +t TOPIC 制限
    try:
        a, _ = new_authed_client("alice8")
        b, _ = new_authed_client("bob8")

        a.send_cmd("JOIN #topictest")
        _ = a.recv_all()
        b.send_cmd("JOIN #topictest")
        _ = b.recv_all()

        a.send_cmd("MODE #topictest +t")
        _ = a.recv_all()

        b.send_cmd("TOPIC #topictest :New Topic")
        resp = b.recv_all()
        ok = (" 482 " in resp) or ("You're not channel operator" in resp)
        record(results, "mode_t_topic_restriction", ok, resp.replace("\n", "\\n"))

        a.close()
        b.close()
    except Exception as e:
        record(results, "mode_t_topic_restriction", False, f"Exception: {e!r}")

    # T13: MODE +k / キーチャンネル
    try:
        a, _ = new_authed_client("alice_k1")
        b, _ = new_authed_client("bob_k1")

        a.send_cmd("JOIN #keychan1")
        _ = a.recv_all()
        a.send_cmd("MODE #keychan1 +k secret")
        _ = a.recv_all()

        # キーなし JOIN -> 475 期待
        b.send_cmd("JOIN #keychan1")
        resp_fail = b.recv_all()
        cond_err = (" 475 " in resp_fail) or ("Cannot join channel (+k)" in resp_fail)

        # 正しいキーで JOIN -> 成功
        b.send_cmd("JOIN #keychan1 secret")
        resp_ok = b.recv_all()
        cond_ok = ("JOIN #keychan1" in resp_ok) or (" 331 " in resp_ok) or (" 353 " in resp_ok)

        ok = cond_err and cond_ok
        info = "join_fail=" + resp_fail.replace("\n", "\\n") + "\njoin_ok=" + resp_ok.replace("\n", "\\n")
        record(results, "mode_k_key_protect", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "mode_k_key_protect", False, f"Exception: {e!r}")

    # T14: MODE +l / ユーザー数制限
    try:
        a, _ = new_authed_client("alice_l1")
        b, _ = new_authed_client("bob_l1")
        c, _ = new_authed_client("carol_l1")

        a.send_cmd("JOIN #limit1")
        _ = a.recv_all()
        a.send_cmd("MODE #limit1 +l 2")
        _ = a.recv_all()

        b.send_cmd("JOIN #limit1")
        _ = b.recv_all()

        # 既に 2 人いる状態で 3 人目が JOIN -> 471 期待
        c.send_cmd("JOIN #limit1")
        resp_fail = c.recv_all()
        ok = (" 471 " in resp_fail) or ("Cannot join channel (+l)" in resp_fail) or ("CHANNELISFULL" in resp_fail)
        record(results, "mode_l_user_limit", ok, resp_fail.replace("\n", "\\n"))

        a.close()
        b.close()
        c.close()
    except Exception as e:
        record(results, "mode_l_user_limit", False, f"Exception: {e!r}")

    # T15: MODE +o / オペレーター付与
    try:
        a, _ = new_authed_client("alice_o1")
        b, _ = new_authed_client("bob_o1")

        a.send_cmd("JOIN #optest1")
        _ = a.recv_all()
        b.send_cmd("JOIN #optest1")
        _ = b.recv_all()

        # まずは bob_o1 の KICK が拒否されることを確認（保険）
        b.send_cmd("KICK #optest1 alice_o1 :Test")
        resp_fail = b.recv_all()

        a.send_cmd("MODE #optest1 +o bob_o1")
        _ = a.recv_all()

        # 付与後に再度 KICK -> 成功するはず
        b.send_cmd("KICK #optest1 alice_o1 :Bye")
        rb = b.recv_all()

        ok_before = (" 482 " in resp_fail) or ("You're not channel operator" in resp_fail)
        ok_after = "KICK #optest1 alice_o1" in rb
        ok = ok_before and ok_after
        info = "before=" + resp_fail.replace("\n", "\\n") + "\nafter=" + rb.replace("\n", "\\n")
        record(results, "mode_o_operator_grant", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "mode_o_operator_grant", False, f"Exception: {e!r}")

    # T16: 部分コマンド PRIVMSG
    try:
        a, _ = new_authed_client("alice9")
        b, _ = new_authed_client("bob9")

        a.send_cmd("JOIN #partial")
        _ = a.recv_all()
        b.send_cmd("JOIN #partial")
        _ = b.recv_all()

        fragments = ["PR", "IVMSG #partial ", ":Hello", " World\r\n"]
        for frag in fragments:
            a.send_raw(frag)
            time.sleep(0.05)

        time.sleep(0.3)
        rb = b.recv_all()
        ok = "PRIVMSG #partial :Hello World" in rb
        record(results, "partial_command_privmsg", ok, rb.replace("\n", "\\n"))

        a.close()
        b.close()
    except Exception as e:
        record(results, "partial_command_privmsg", False, f"Exception: {e!r}")

    # T17: QUIT によるクリーンな切断
    try:
        a, _ = new_authed_client("alice10")
        b, _ = new_authed_client("bob10")

        a.send_cmd("JOIN #quitchan")
        _ = a.recv_all()
        b.send_cmd("JOIN #quitchan")
        _ = b.recv_all()

        a.send_cmd("QUIT :Leaving")
        ra = a.recv_all()
        time.sleep(0.3)
        rb = b.recv_all()

        ok = ("QUIT :Leaving" in rb) or ("QUIT" in rb)
        info = "alice_view=" + ra.replace("\n", "\\n") + "\nbob_view=" + rb.replace("\n", "\\n")
        record(results, "quit_cleanup", ok, info)

        a.close()
        b.close()
    except Exception as e:
        record(results, "quit_cleanup", False, f"Exception: {e!r}")

    return results


def main() -> int:
    print(f"Connecting to {HOST}:{PORT} with password '{PASSWORD}'", file=sys.stderr)
    results = run_tests()
    passed = sum(1 for _, ok, _ in results if ok)
    total = len(results)

    print("TEST_RESULTS_START")
    for name, ok, info in results:
        status = "OK" if ok else "NG"
        print(f"[{status}] {name}")
        s = str(info)
        if len(s) > 600:
            s = s[:600] + "...(truncated)"
        print("  ", s.replace("\n", "\\n"))
    print(f"SUMMARY {passed} / {total}")
    print("TEST_RESULTS_END")

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())


