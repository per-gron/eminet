var EmiNet = require('./eminet');

var mediator = EmiNet.openMediator({
      address: '127.0.0.1',
      type: 'udp4',
      port: 4001
    }),
    es1      = EmiNet.open(),
    es2      = EmiNet.open();

console.log("Mediator address:", mediator.getAddress()+':'+mediator.getPort());

var cookiePair   = mediator.generateCookiePair(),
    sharedSecret = mediator.generateSharedSecret();

console.log("A cookie pair:", cookiePair);
console.log("A shared secret:", sharedSecret);

var connect = function(socket, cookie, name) {
  socket.connectP2P(mediator.getAddress(), mediator.getPort(), cookie, sharedSecret, function(err, conn) {
    if (null !== err) {
      return console.log(name+' connection failed');
    }
    
    console.log(name+" connected", arguments);
    
    conn.on('message', function(channelQualifier, buf) {
      console.log(name+" got message", buf.toString());
    });
    
    conn.on('p2p', function(error) {
      if (null !== err) {
        return console.log(name+": Failed to establish P2P connection");
      }
      
      console.log(name+": P2P connection established");
    })
    
    setTimeout(function() {
      console.log("Sending message from "+name);
      conn.send(new Buffer("Hej"));
    }, 2000);
  });
};

connect(es1, cookiePair[0], 'ES1');
connect(es2, cookiePair[1], 'ES2');
