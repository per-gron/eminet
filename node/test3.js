var EmiNet = require('./eminet');

var mediator = EmiNet.openMediator({ port: 4001 }),
    es1      = EmiNet.open(),
    es2      = EmiNet.open();

console.log("Mediator address:", mediator.getAddress()+':'+mediator.getPort());

console.log("A cookie:", mediator.generateCookie());
console.log("A shared secret:", mediator.generateSharedSecret());
