  var wsUri = "ws://"+window.location.host+"/ws.cgi";
  var output;

  var websocket;
  var textarea;
  var reconnectTimer;

  function WSConnect() {
    websocket = new WebSocket(wsUri);
    websocket.onopen = function(evt) { onOpen(evt) };
    websocket.onclose = function(evt) { onClose(evt) };
    websocket.onmessage = function(evt) { onMessage(evt) };
    websocket.onerror = function(evt) { onError(evt) };
  }

  function onOpen(evt) {
    writeToScreen("CONNECTED");
    doSend("WSConnected");
    clearInterval(reconnectTimer);
  }

  function onClose(evt) {
    writeToScreen("DISCONNECTED\n");
    reconnectTimer=setTimeout(function() {
      testWebSocket();
    }, 1000);
  }

  function onMessage(evt) {
    writeToScreen('RECEIVED: ' +evt.data);
  }

  function onError(evt) {
    writeToScreen('ERROR: '+evt.data);
  }

  function doSend(message) {
    writeToScreen("SENT: " + message); 
    websocket.send(message);
  }

  function writeToScreen(message) {
    var newVal=textarea.value+'\n'+message;
    textarea.value=newVal;
    textarea.scrollTop = textarea.scrollHeight;
  }

  function init()
  {
    output = document.getElementById("output");
    WSConnect();
    textarea = document.getElementById('textarea_id');
//    setInterval(function(){
//      textarea.scrollTop = textarea.scrollHeight;
//    }, 1000);
  }
  window.addEventListener("load", init, false);