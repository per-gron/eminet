// CXX=clang node-waf configure build && node eminet.js

var EmiNetAddon = require('./build/Release/eminet');

// lazily loaded
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

var connectionDisconnect = function() {
    // TODO
    console.log("!!! Connection disconnect", arguments);
};

EmiNetAddon.setCallbacks(
    gotConnection,
    connectionMessage,
    connectionLost,
    connectionRegained,
    connectionDisconnect
);

EmiNetAddon.EmiSocket.prototype.connect = function(address, port, cb) {
    var type = this.type || 'udp4';
    
    if ('udp4' == type) {
        return this.connect4(address, port, cb);
    }
    else if ('udp6' == type) {
        return this.connect6(address, port, cb);
    }
    else {
        throw new Error('Bad socket type. Valid types: udp4, udp6');
    }
};

var open = function(args) {
    var s = new EmiNetAddon.EmiSocket();
    
    for (var key in args) {
        s[key] = args[key];
    }
    
    Object.freeze(s);
    
    return s;
};

var es2 = open();
var es = open({
    acceptConnections: true,
    port: 5001
});

es2.connect('127.0.0.1', 5001, function(err, socket) {
    console.log("!!! Connected", arguments);
});


console.log("Hej", es, es2);
