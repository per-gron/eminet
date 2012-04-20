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

var gotConnection = function() {
    // TODO
    console.log("!!! Got connection", arguments);
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

var EmiConnection = function(sockHandle, address, port, cb) {
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
};

Util.inherits(EmiConnection, Events.EventEmitter);

EmiConnection.prototype.close = function() {
  return this._handle.close.apply(this._handle, arguments);
};

EmiNetAddon.EmiSocket.prototype.connect = function(address, port, cb) {
  return new EmiConnection(this, address, port, cb);
};

exports.open = function(args) {
    var s = new EmiNetAddon.EmiSocket(args);
    
    for (var key in args) {
        s[key] = args[key];
    }
    
    Object.freeze(s);
    
    return s;
};
