var EmiNet = require('./eminet');

var mediator = EmiNet.openMediator({ port: 4001 }),
    es1      = EmiNet.open(),
    es2      = EmiNet.open();

console.log(mediator.getAddress(), mediator.getPort());