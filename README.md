# ft_irc - IRC Server

An IRC server implemented in C++98, built on a single `poll()` event loop with non-blocking sockets.

## Overview

This project implements an IRC server that speaks a subset of the IRC protocol (as defined by RFC 1459/2812). It accepts multiple concurrent client connections and multiplexes them over a single `poll()` call, without threads or blocking I/O. Clients authenticate with `PASS`/`NICK`/`USER`, join channels, exchange messages, and channel operators can manage membership and channel modes.

## Features

### Authentication

- `PASS` - server password authentication
- `NICK` - nickname registration/change
- `USER` - user information registration

### Channel Management

- `JOIN` - join a channel
- `PART` - leave a channel

### Messaging

- `PRIVMSG` - send a private/channel message
- `NOTICE` - send a notice (no error reply is sent back)

### Operator Commands

- `KICK` - remove a user from a channel
- `INVITE` - invite a user to a channel
- `TOPIC` - view/change the channel topic
- `MODE` - change channel modes
  - `i` - invite-only
  - `t` - topic restricted to operators
  - `k` - channel key (password)
  - `o` - operator privilege
  - `l` - user limit

### Other

- `PING`/`PONG` - connection keep-alive
- `QUIT` - disconnect

## Architecture & Implementation

### `poll()`-based event loop

`Server::start()` runs a single loop around one `poll()` call covering the listening socket and every connected client fd (`std::vector<struct pollfd>`). Each iteration:

- `POLLIN` on the listening fd triggers `handleNewConnection()`, which `accept()`s the connection, sets it non-blocking with `fcntl(fd, F_SETFL, O_NONBLOCK)`, wraps it in a `Client`, and registers it in `_pollFds` with `POLLIN | POLLOUT`. `POLLOUT` remains registered even when the send buffer is empty; in that case `handleClientSend()` returns immediately.
- `POLLIN` on a client fd triggers `handleClientData()`, which reads into a fixed buffer with `recv()` and appends the data to the client's receive buffer.
- `POLLOUT` on a client fd triggers `handleClientSend()`, which flushes as much of the client's send buffer as `send()` accepts.
- `POLLERR`/`POLLHUP`/`POLLNVAL` closes and removes the connection.

All sockets are non-blocking, so `recv()`/`send()` errors of `EAGAIN`/`EWOULDBLOCK` are treated as "try again later" rather than fatal errors. Other send errors, including `EINTR` in the current implementation, close the client connection. There is no per-client thread or blocking call anywhere in the loop.

### Client state machine and message framing

`Client` tracks registration progress with three flags (`_hasPassword`, `_hasNick`, `_hasUser`) plus a final `_isAuthenticated` flag. Each of `PASS`, `NICK`, and `USER` can arrive in any order; after each one, the handler checks whether all three prerequisites are met and, if so, marks the client authenticated and sends the welcome reply. Any command other than `PASS`/`NICK`/`USER`/`PING`/`QUIT` is rejected with `ERR_NOTREGISTERED` until authentication completes.

Because TCP is a byte stream, a single `recv()` call may deliver a partial line or several lines at once. Incoming bytes are appended to a per-client receive buffer; `Client::hasCompleteMessage()`/`extractMessage()` scan for `\n` (stripping a preceding `\r`) and only hand a complete line to the command dispatcher, leaving any remainder buffered for the next `recv()`. Outgoing data goes through the same pattern in reverse: `Server::sendToClient()` appends to a per-client send buffer (capped at 16 KiB — the connection is dropped if this limit is exceeded) and `handleClientSend()` drains it on `POLLOUT`, so writes never block the event loop even if a client reads slowly.

### Message parsing and command dispatch

`Message::parse()` implements the IRC line grammar `[:prefix] <command> <params...> [:trailing]`: an optional `:`-prefixed source, a command token, space-separated middle parameters, and an optional trailing parameter introduced by `:` that may itself contain spaces.

`CommandHandler::execute()` upper-cases the command and dispatches it through a fixed if/else chain grouped by responsibility, each group implemented in its own translation unit under `srcs/commands/`: `auth.cpp` (`PASS`/`NICK`/`USER`), `channel.cpp` (`JOIN`/`PART`), `message.cpp` (`PRIVMSG`/`NOTICE`), and `operator.cpp` (`KICK`/`INVITE`/`TOPIC`/`MODE`). `PING`, `QUIT`, and the authentication commands are dispatched before the authentication check so an unregistered client can still complete the handshake or disconnect; every other command falls through to the authentication gate first. Unknown commands after authentication receive `ERR_UNKNOWNCOMMAND`.

### Class overview

- `Server` - owns the listening socket, the `pollfd` vector, the client and channel maps, and drives the event loop.
- `Client` - per-connection state: identity (nickname/username/hostname/realname), registration flags, and receive/send buffers.
- `Channel` - membership (`std::vector<Client*>`), operators (`std::set<Client*>`), invite list, topic, key, and user limit.
- `CommandHandler` - parses no state of its own; translates a `Message` plus the issuing `Client` into protocol replies via `Server`.
- `Message` - parses a single raw IRC line into prefix/command/params/trailing and formats numeric replies.

## Build & Usage

### Build

```bash
make
```

Compiled with `c++ -Wall -Wextra -Werror -std=c++98`.

### Run

```bash
./ircserv <port> <password>
```

Example:

```bash
./ircserv 6667 local-dev-password
```

> The password is passed as a command-line argument and can appear in shell history and process
> listings. Use the example only for local testing, and never reuse a real credential.

### Testing with `nc`

```bash
nc localhost 6667
PASS local-dev-password
NICK testnick
USER testuser 0 * :Test User
JOIN #test
PRIVMSG #test :Hello, world!
QUIT
```

### Testing partial/fragmented commands

```bash
nc -C 127.0.0.1 6667
PASS^D
mypass^D
word^D
<Enter>
```

### Testing with `irssi`

```bash
irssi
/connect localhost 6667 mypassword
/nick mynick
/join #test
/msg #test Hello!
```

## Notes

- Server-to-server linking is not implemented.
- File transfer (DCC, bonus) is not implemented.
- Bot mode (bonus) is not implemented.
