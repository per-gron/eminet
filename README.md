# EmiNet

EmiNet is a UDP based network protocol designed to fit the needs of games.

The congestion control algorithms are designed with mobile networks in mind, and are based on UDT. See [UDT's website](http://udt.sourceforge.net). I used [RakNet's description of the algorithms](http://www.jenkinssoftware.com/raknet/manual/congestioncontrol.html) heavily when designing and implementing them for EmiNet. Also, see [here](http://www.jenkinssoftware.com/forum/index.php?topic=2624.0) and [here](http://www.rakkar.org/blog/?p=443).

The NAT punch through handshake is designed so that it first sets up a proxy connection and then the NAT punch through is attempted. If the punch through fails, the proxy connection is already there, and the connection will work without any special case in the code and even without any connection opening time penalty. I primarily studied [this page](http://www.brynosaurus.com/pub/net/p2pnat/) when designing this part of the protocol.


## Conceptual overview

EmiNet is connection based. This means that to send data, you first need to open a connection, which EmiNet will maintain for you. In this sense, EmiNet feels more like TCP than UDP.

EmiNet deals with discrete messages rather than a continuous stream of bytes. In this sense, EmiNet is more like UDP than TCP. Messages are binary blobs. EmiNet does not impose any limitation on the message format except that messages must be of non-zero length.

EmiNet supports three kinds of connections: server, client and P2P. The API for opening a connection is different depending on what type of connection you need, but once a connection is established the API is the same for all types of connections.

### Heartbeats

EmiNet has built-in heartbeat logic: It can be configured to send an empty heartbeat at `heartbeatFrequency` Hz. If no messages are received from the other host after we expect `heartbeatsBeforeConnectionWarning` heartbeats, a connection warning is issued. If no messages are received in `connectionTimeout` seconds, the connection is lost. This is different from TCP, which does not have this functionality built-in.

For latency sensitive mobile games, heartbeats are useful not only for detecting connection loss, but also to maintain the connection: According to my measurements, HDSPA+ connections are capable of round trip times Uppsala<->London of ~100ms, where deviations >10ms are rare. However, this only happens when packets are sent at least once every 150ms or so. When packets are sent less frequently, latency varies wildly between 300ms-1.5s. Fast heartbeats can help keeping the latency low (but will of course drain battery and bandwidth).

### Channels

Each EmiNet connection has a number of independent *channels*. Channels are a convenient feature that can be used for instance to separate game data from VoIP data. There are four types of channels:

1. **Unreliable**: Messages can arrive out of order, in duplicates, and might get discarded. This is very similar to raw UDP.
2. **Unreliable sequenced**: Messages might get discarded, but they never arrive out of order or as duplicates (if they do, they are discarded).
3. **Reliable sequenced**: Messages might get discarded, but they never arrive out of order or as duplicates (if they do, they are discarded). EmiNet will re-send the last message until it is acknowledged. This is useful when only the most recent information is relevant, for instance the position of a player.
4. **Reliable ordered**: No message is discarded, and they are guaranteed to arrive in order. This provides essentially the same guarantees (and latency issues) as TCP.

There are 32 channels of each type. Channels don't need to be initialized or closed: to send a message over a channel, just do it.

### Bundled messages

In order to minimize network overhead, EmiNet attempts to bundle together multiple messages into one packet: When EmiNet is instructed to send a message, it will not send it immediately. Rather, it starts a timer (but only if it's not already running) that fires after one "tick", which is 10ms. When the tick timer fires, enqueued messages are grouped together and sent.

### Message priority

Each message has a priority associated with it. There are four priorities: `immediate`, `high`, `medium` and `low`. Messages with the `immediate` priority are sent immediately, bypassing the tick timer. For every message of a given priority, two messages of the priority one step above will be sent (if there are any). The behavior when having messages with different priorities in the same channel in the send queue is unspecified: It is recommended to always use the same priority for each channel.

### Two-way server-client handshake

When opening client-server connections, EmiNet uses a two-way handshake. This makes opening connections faster than TCP's three-way handshake, which is especially important over networks like 3G, that always have high latency and extra high latency before a connection has been established. The drawback of the two-way handshake is that if packets are lost or duplicated, the server might receive connections that are dead from the start. In order to avoid DoS vulnerabilities, care must be taken to not allocate any resources until the first message is received on a server connection. P2P connections employ a much more complicated handshake and does not have this issue.

### Long messages

Messages that are too large to fit in a UDP packet are automatically split up and sent in separate packets. However, please note that unreliable channels do not do anything to re-send parts of split messages, so the probability of messages being lost increase exponentially to the number of splits. For messages longer than 1-2KB or so, I'd recommend using a reliable channel.

### P2P

In order to initiate a P2P connection, a third party *mediator* is required. The mediator must have a public IP and port, and must not be behind NAT. The mediator aids in the NAT punch through process and acts as a proxy (possibly with a rate limit for each connection) if necessary. The steps to set up a P2P connection are:

1. The mediator generates a *cookie pair* and a *shared secret*.
2. The shared secret and a cookie is sent to each of the peers that will initiate a P2P connection. This is normally done through other means than EmiNet itself.
3. Each peer connects. The P2P connect function takes the cookie, the shared secret, the mediator's IP and the mediator port number as parameters.

The API for setting up a mediator is currently only exposed through the node.js bindings, and are not available with Objective-C.


## API

EmiNet itself is implemented in C++, and there are currently node.js and Objective-C bindings. Please note that there are currently no C++ bindings; there is currently no way of using EmiNet from C++.

This is a brief language agnostic overview of the EmiNet API. For more details, please refer to the source code.

The EmiNet API consists of two main classes: EmiSocket and EmiConnection.

### EmiSocket

`EmiSocket` is the main entry point of the EmiNet API. To do anything (except for creating a P2P mediator), an `EmiSocket` object must be created. `EmiSocket` objects don't represent actual connections, they only contain various configuration parameters and, if configured to accept connections, callbacks for receiving connections.

To see what configuration options are available, please refer to `EmiSockConfig.h`.

The two main operations on a `EmiSocket` object are connect to server and P2P connect.

### EmiConnection

An `EmiConnection` object represents an EmiNet connection.

* `close` closes the connection, and attempts to notify the other host about it.
* `forceClose` closes the connection without notifying the other host.
* `send` sends a message. The parameters to this method are the data to send, the channel qualifier (see `EMI_CHANNEL_QUALIFIER`) and the message priority.

The events that an `EmiConnection` object might emit are

* `message`: A message was received
* `lost`: Connection lost warning
* `regained`: The connection was regained (opposite of `lost`)
* `disconnect`: The connection was closed, either because of an error or because one side closed the connection.
* `p2p`: The NAT punch through succeeded or failed (in which case the connection falls back on proxying).


## Code structure

There are three source code directories in the EmiNet distribution: `core`, `node` and `objc`. As the names imply, they are for the core logic, the node.js bindings and the Objective-C bindings, respectively.

EmiNet is structured in a rather special way: The `core` code is designed to be completely runtime, language and OS agnostic. It does not directly use timers or network APIs, and it's designed to be usable regardless of which memory or concurrency model the surface API language uses. It is not intended to be used directly, only through wrappers. This is the reason why you can't currently use EmiNet directly from C++: To do that, someone would have to write a C++ wrapper for it.

In some ways, the code becomes a little bit awkward because of this, but there are several major gains:

**Memory management**: When using EmiNet in node.js, EmiNet objects are garbage collected just like you'd expect from a Javascript library. When using the Objective-C wrapper, EmiNet objects are reference counted, just like they should be.

**Network APIs**: When using node.js EmiNet, EmiNet uses node.js' libuv library for network I/O and timers. This means that it is perfectly integrated with the node.js runloop. For instance, if you open a server socket to listen for clients and return from the main script, the application will continue running, because libuv detects that there's something waiting on a socket. Conversely, when using the Objective-C bindings, EmiNet uses native iOS networking APIs and GCD timers that integrate perfectly with iOS' concurrency model.

**Concurrency**: node.js EmiNet embraces the Javascript concurrency model: there is no concurrency. Javascript users of EmiNet can thus enjoy the simplicity of not having to worry about most preemptive concurrency issues and lock performance problems. Objective-C EmiNet is fully integrated with GCD, and is capable of running each connection on a separate queue if you need to squeeze multi-core performance. If you don't need that, it's also very easy to run all EmiNet logic on the main runloop.


## Usage

There isn't much documentation for actual EmiNet usage. Fortunately, the API is rather small.

**Objective-C**: `EmiNet.h` is the header that should be `#import`ed. `EmiSocket.h` and `EmiConnection.h` are fairly easy to grok and should give a general impression on how to use the library. Please note that `init`ing an `EmiSocket` object is not enough; it has to be started before use, with `startWithError:` or `startWithConfig:error:`.

Objective-C EmiNet employs a GCD based concurrency model inspired by CocoaAsyncSocket. For more information, please refer to the [CocoaAsyncSocket wiki](https://github.com/robbiehanson/CocoaAsyncSocket/wiki/Reference_GCDAsyncSocket).

**node.js**: Check out the `node/test*.js` files. They are examples of how to use EmiNet, and actually use a rather large proportion of the API.


## Installation

To use the Objective-C wrapper in Xcode, simply add the files in the `objc` and `core` directories to the project (within groups, not folders). The Objective-C wrapper depends on the excellent [CocoaAsyncSocket](https://github.com/robbiehanson/CocoaAsyncSocket) library.

To use the Javascript wrapper, add an npm dependency in your project's `package.json` like so: `"eminet": "git+ssh://git@bitbucket.org:per_eckerdal/eminet.git"`
