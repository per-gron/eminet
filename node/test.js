var EmiNet = require('./eminet');

var es2 = EmiNet.open();
var es = EmiNet.open({
    acceptConnections: true,
    port: 5001
});

es.on('connection', function(socket) {
  console.log("-- Client connected");
  
  socket.on('disconnect', function() {
    console.log("-- Client disconnected");
  });
  
  socket.on('lost', function() {
    console.log("-- Client connection lost");
  });
  
  socket.on('regained', function() {
    console.log("-- Client connection regained");
  });
});

es2.connect('127.0.0.1', 5001, function(err, socket) {
  if (null !== err) {
    return console.log("-- Failed to connect", err);
  }
  
  socket.on('disconnect', function() {
    console.log("-- Disconnected");
  })
  
  console.log("-- Connected", socket);
  socket.forceClose();
});


console.log("Hej", es, es2);

