// This is useful for testing the reliability of opening connections

var EmiNet = require('./eminet');

var packetLoss = 0.3;
var packetLossPerSocket = 1-Math.sqrt(1-packetLoss);

var es = EmiNet.open({ fabricatedPacketDropRate: packetLossPerSocket }),
    es2 = EmiNet.open({
      acceptConnections: true, port: 2345,
      fabricatedPacketDropRate: packetLossPerSocket,
      rateLimit: 1000
    });

var open = function(msg, cb) {
  var callbackWasInvoked = false;
  
  es.connect('127.0.0.1', 2345, function(err, socket) {
    if (callbackWasInvoked) {
      console.log("Callback has already been invoked!", err, socket);
      throw new Error("Callback has already been invoked!");
    }
    callbackWasInvoked = true;
    
    if (err) {
      // TODO This callback seems to be called more than once per call to connect, first with success, then with errors.
      console.log("ERR 1:", err);
      return;
    }
    
    socket.on('disconnect', function(reason) {
      if (EmiNet.THIS_HOST_CLOSED == reason) {
        cb();
      }
      else {
        console.log("Client "+msg+" lost connection", reason);
      }
    });
    
    socket.on('message', function(channelQualifier, buf, offset, len) {
      console.log("Client got message", buf.toString('utf8', offset, offset+len));
    });
    
    socket.on('lost', function() {
      console.log("Client "+msg+": LOST!");
    });
    
    socket.on('regained', function() {
      console.log("Client "+msg+": REGAINED!");
    });
    
    socket.close();
  });
};

var arr = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
           'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
           'w', 'x', 'y', 'z', '1', '2', '3', '4', '5', '6', '7',
           '8', '9', '0'];
// arr = ['a'];
var success = 0;
arr.forEach(function(name) {
  open(name, function() {
    success += 1;
    if (success == arr.length) {
      console.log("SUCCESS!!!");
    }
    else {
      console.log("Remaining:", arr.length - success);
    }
  });
});
