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
  sock.emit('connection', conn);
  return conn;
};

var connectionMessage = function() {
    // TODO
    console.log("!!! Connection message", arguments);
};

var connectionLost = function() {
    // TODO
    console.log("!!! Connection lost", arguments);
};

var connectionRegained = function() {
    // TODO
    console.log("!!! Connection regained", arguments);
};

var connectionDisconnect = function(conn, reason) {
  conn.emit('disconnect', reason);
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
      cb(err, conn && self);
      
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

EmiConnection.prototype.close = function() {
  return this._handle.close.apply(this._handle, arguments);
};


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


exports.open = function(args) {
  return new EmiSocket(args);
};
