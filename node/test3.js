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

es1.connectP2P(mediator.getAddress(), mediator.getPort(), cookie, sharedSecret, function() {
  console.log("ES1 connected", arguments);
});

es2.connectP2P(mediator.getAddress(), mediator.getPort(), cookie, sharedSecret, function() {
  console.log("ES2 connected", arguments);
});
