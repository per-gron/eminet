// CXX=clang node-waf configure build && node test.js

var Util = require('util'),
    Events = require('events'),
    EmiNetAddon = require('./build/Release/eminet');

// Lazily loaded
var Dns = null,
    Net = null;

var isIP = function(address) {
    if (!net) net = require('net');
    return net.isIP(address);
};

var lookup = function(address, family, callback) {
    // implicit 'bind before send' needs to run on the same tick
    var matchedFamily = isIP(address);
    if (matchedFamily)
        return callback(null, address, matchedFamily);

    if (!dns)
        dns = require('dns');

    return dns.lookup(address, family, callback);
};

var errnoException = function(errorno, syscall) {
    var e = new Error(syscall+' '+errorno);
    e.errno = e.code = errorno;
    e.syscall = syscall;
    return e;
};

var lookup4 = function(address, callback) {
    return lookup(address || '0.0.0.0', 4, callback);
};

var lookup6 = function(address, callback) {
    return lookup(address || '::0', 6, callback);
};

var gotConnection = function(sock, sockHandle, connHandle) {
  var conn = new EmiConnection(false, sockHandle, connHandle);
  sock && sock.emit('connection', conn);
  return conn;
};

var connectionMessage = function(conn, connHandle, channelQualifier, slowBuffer, offset, length) {
  conn && conn.emit('message', channelQualifier, new Buffer(slowBuffer, length, offset));
};

var connectionLost = function(conn, connHandle) {
  conn && conn.emit('lost');
};

var connectionRegained = function(conn, connHandle) {
  conn && conn.emit('regained');
};

var connectionDisconnect = function(conn, reason) {
  conn && conn.emit('disconnect', reason);
};

var connectionError = function() {
  // TODO
  console.log("!!! Connection error", arguments);
};

EmiNetAddon.setCallbacks(
    gotConnection,
    connectionMessage,
    connectionLost,
    connectionRegained,
    connectionDisconnect,
    connectionError
);


var EmiConnection = function(initiator, sockHandle, address, port, cb) {
  if (initiator) {
    var self = this;
    
    var type = this.type || 'udp4';
    
    var wrappedCb = function(err, conn) {
      self._handle = conn;
      
      // We want to wait with invoking cb until this function
      // has returned, to make sure that the C++ wrapper code
      // has access to the self object when cb is invoked,
      // which might be necessary for instance if the callback
      // invokes forceClose.
      process.nextTick(function() {
        cb(err, conn && self);
      });
      
      return self;
    };
    
    if ('udp4' == type) {
        sockHandle.connect4(address, port, wrappedCb);
    }
    else if ('udp6' == type) {
        sockHandle.connect6(address, port, wrappedCb);
    }
    else {
        throw new Error('Bad socket type. Valid types: udp4, udp6');
    }
  }
  else {
    this._handle = address;
  }
};

Util.inherits(EmiConnection, Events.EventEmitter);

[
  'close', 'forceClose', 'closeOrForceClose', 'flush', 'send',
  'hasIssuedConnectionWarning', 'getSocket', 'getAddressType',
  'getPort', 'getAddress', 'getInboundPort', 'isOpen', 'isOpening'
].forEach(function(name) {
  EmiConnection.prototype[name] = function() {
    return this._handle[name].apply(this._handle, arguments);
  };
});


var EmiSocket = function(args) {
  this._handle = new EmiNetAddon.EmiSocket(this, args);
  
  for (var key in args) {
    this[key] = args[key];
  }
};

Util.inherits(EmiSocket, Events.EventEmitter);

EmiSocket.prototype.connect = function(address, port, cb) {
  return new EmiConnection(/*initiator:*/true, this._handle, address, port, cb);
};

// TODO suspend doesn't seem to work when you call it on a disconnect
// callback. I suspect this is a libuv bug, because the assert that
// triggers is removed by this commit:
// https://github.com/joyent/libuv/commit/28b0867f0312e3ccdc45d7fde0218d065ed40c65
EmiSocket.prototype.suspend = function() {
  return this._handle.suspend.apply(this._handle, arguments);
};

EmiSocket.prototype.desuspend = function() {
  return this._handle.desuspend.apply(this._handle, arguments);
};


exports.open = function(args) {
  return new EmiSocket(args);
};
