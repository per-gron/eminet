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

es2.on('connection', function(socket) {
  console.log("                                  Server: Got connection");
  
  socket.on('disconnect', function(reason) {
    if (EmiNet.OTHER_HOST_CLOSED == reason) {
      console.log("                                  Server: Client closed the connection");
    }
    else {
      console.log("                                  Server: Lost connection", reason);
    }
  });
});

var open = function(msg) {
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
    
    console.log("Client "+msg+": Connected");
    
    socket.on('disconnect', function(reason) {
      if (EmiNet.THIS_HOST_CLOSED == reason) {
        console.log("Client "+msg+": Connection successfully closed");
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

open('a');
/*open('b');
open('c');
open('d');
open('1');
open('2');
open('3');
open('4');
open('A');
open('B');
open('C');
open('D'); */