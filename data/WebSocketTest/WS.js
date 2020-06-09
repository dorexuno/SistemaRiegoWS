var dir = "ws://"+location.hostname+":81/";
var connection = new WebSocket(dir,['arduino']);

connection.onopen = function () {
    connection.send('Connect ' + new Date());
    console.log('WebSocket connection openned in '+dir);
  };
connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
  };
connection.onmessage = function (e) {
    console.log('Server: ', e.data);
  };
connection.onclose = function () {
    console.log('WebSocket connection closed');
  };