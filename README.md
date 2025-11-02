# ft_irc - IRC Server

C++98標準で実装されたIRCサーバー

## 概要

このプロジェクトは、Internet Relay Chat (IRC) プロトコルに準拠したサーバーの実装です。非ブロッキングI/Oとpoll()を使用して、複数のクライアント接続を同時に処理します。

## 機能

### 認証
- `PASS` - サーバーパスワード認証
- `NICK` - ニックネーム設定
- `USER` - ユーザー情報設定

### チャンネル管理
- `JOIN` - チャンネル参加
- `PART` - チャンネル退出

### メッセージング
- `PRIVMSG` - プライベートメッセージ送信
- `NOTICE` - 通知送信（エラー応答なし）

### オペレーターコマンド
- `KICK` - ユーザー追放
- `INVITE` - ユーザー招待
- `TOPIC` - トピック表示/変更
- `MODE` - チャンネルモード変更
  - `i` - 招待制
  - `t` - トピック制限
  - `k` - チャンネルキー
  - `o` - オペレーター権限
  - `l` - ユーザー数制限

### その他
- `PING/PONG` - 接続維持
- `QUIT` - 切断

## ビルド

```bash
make
```

## 使用方法

```bash
./ircserv <port> <password>
```

例：
```bash
./ircserv 6667 mypassword
```

## テスト

### ncを使用したテスト

```bash
nc localhost 6667
PASS mypassword
NICK testnick
USER testuser 0 * :Test User
JOIN #test
PRIVMSG #test :Hello, world!
QUIT
```

### 部分コマンドテスト

```bash
nc -C 127.0.0.1 6667
PASS^D
mypass^D
word^D
<Enter>
```

### irssiを使用したテスト

```bash
irssi
/connect localhost 6667 mypassword
/nick mynick
/join #test
/msg #test Hello!
```

## 技術仕様

- **C++標準**: C++98
- **コンパイラフラグ**: `-Wall -Wextra -Werror -std=c++98`
- **I/O**: 非ブロッキング、poll()使用（1つのみ）
- **プロトコル**: IRC (RFC 1459/2812準拠)

## プロジェクト構造

```
ft_irc/
├── Makefile
├── includes/          # ヘッダーファイル
│   ├── Server.hpp
│   ├── Client.hpp
│   ├── Channel.hpp
│   ├── CommandHandler.hpp
│   └── Message.hpp
├── srcs/             # ソースファイル
│   ├── main.cpp
│   ├── Server.cpp
│   ├── Client.cpp
│   ├── Channel.cpp
│   ├── CommandHandler.cpp
│   ├── Message.cpp
│   └── commands/     # コマンド実装
│       ├── auth.cpp
│       ├── channel.cpp
│       ├── message.cpp
│       └── operator.cpp
└── README.md
```

## 追記事項

- サーバー間通信は非対応
- ファイル転送は非対応（ボーナス）
- ボット機能は非対応（ボーナス）
