var EmiNet = require('./eminet');

var mediator = EmiNet.openMediator({
      address: '127.0.0.1',
      type: 'udp4',
      port: 4001
    }),
    es1      = EmiNet.open(),
    es2      = EmiNet.open();

console.log("Mediator address:", mediator.getAddress()+':'+mediator.getPort());

var cookie       = mediator.generateCookie(),
    sharedSecret = mediator.generateSharedSecret();

console.log("A cookie:", cookie);
console.log("A shared secret:", sharedSecret);

var connect = function(socket, name) {
  socket.connectP2P(mediator.getAddress(), mediator.getPort(), cookie, sharedSecret, function(err, conn) {
    if (null !== err) {
      return console.log(name+' connection failed');
    }
    
    console.log(name+" connected", arguments);
    
    socket.on('message', function(channelQualifier, buf) {
      console.log(name+" got message", buf.toString());
    });
    
    setTimeout(function() {
      console.log("Sending message from "+name);
      conn.send(new Buffer("Hej"));
    }, 2000);
  });
};

connect(es1, 'ES1');
connect(es2, 'ES2');
