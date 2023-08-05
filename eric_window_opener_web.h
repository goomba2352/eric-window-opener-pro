#ifndef ERIC_WINDOW_OPENER_WEB_H
#define ERIC_WINDOW_OPENER_WEB_H

#include <ESP8266WebServer.h>

// ==================================  common  ==================================
const char common_head1[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<style type="text/css">a {
  color: #cccccc;
  font-family: monospace;
}
a:visited {
  color: #cccccc;
  font-family: monospace;
}
body {
  background-color: #4287f5;
  color: #ffffff;
  font-family: Arial;
}
.the-button {
  background-color: #204580;
  border: none;
  color: white;
  padding: 15px 32px;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 30px;
  width: 60%;
  height: 40vh;
}
.settings input {
background-color: #204580;
  border: none;
  color: white;
  width:100px;
}
.settings input[type=number] {
  font-family: monospace;
}
.settings tr {
  border-bottom: 1pt solid #fff;
}

.settings td {
  padding:5px;
}
table {
  border-collapse: collapse;
}
</style>
<title>
)=====";
const char common_head2[] PROGMEM = R"=====(
</title>
<body>
<table><tr><td><a href="/">Home</a></td><td><a href="/settings">Settings</a></td></tr></table>
<hr>
)=====";

const char footer[] PROGMEM = R"=====(
</body></html>
)=====";

// ==================================  home page  ==================================

const char home[] PROGMEM = R"=====(<center>
<div>
<h1>Eric Window Opener Pro</h1>
  <button class="the-button" onclick="buttonPress()" id="btn"></button>
</div>
<br>
<div><h2>
  Close Sensor: <span id="close_sensor">NA</span>
</h2>
<h2>
  Open Sensor: <span id="open_sensor">NA</span>
</h2>
<h2>
  Button Sensor: <span id="button_sensor">NA</span>
</h2>
<h2>
  State: <span id="state_name">NA</span>
</h2>
</div>
</center>
<script>
function buttonPress() {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "button", true);
  xhttp.send();
}
setInterval(function() { getData();}, 100); 
function getData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      response=JSON.parse(this.responseText);
      document.getElementById("close_sensor").innerHTML = response.close_sensor;
      document.getElementById("open_sensor").innerHTML = response.open_sensor;
      document.getElementById("state_name").innerHTML = response.state_name;
      document.getElementById("btn").innerHTML = response.button_action;
      document.getElementById("button_sensor").innerHTML = response.button_sensor;
    }
  };
  xhttp.open("GET", "raw", true);
  xhttp.send();
}
</script>
)=====";

// ================================== /settings ==================================

const char settings_page[] PROGMEM = R"=====(
    <script>
var xhttp = new XMLHttpRequest();
xhttp.onreadystatechange = function() {
  if (this.readyState == 4 && this.status == 200) {
    response=JSON.parse(this.responseText);
    document.getElementById("open_timeout").value = response.open_timeout;
    document.getElementById("close_timeout").value = response.close_timeout;
    document.getElementById("open_secure").value = response.open_secure;
    document.getElementById("close_secure").value = response.close_secure;
    document.getElementById("open_secure_speed").value = response.open_secure_speed;
    document.getElementById("close_secure_speed").value = response.close_secure_speed;
    document.getElementById("default_speed").innerHTML = response.default_motor_speed;
  }
};
xhttp.open("GET", "get_settings", true);
xhttp.send();
    </script>
    <table class="settings">
      <form action="settings" method="post">
    <tr><td><span>Open Timeout (ms): </span></td><td>
      <input type="number" name="open_timeout" id = "open_timeout" min=200 max=300000></td></tr>
    <tr><td><span>Open sensor to close (ms): </span></td><td>
      <input type="number" name="open_secure" id = "open_secure" min=200 max=20000></td></tr>
    <tr><td><span>Open secure speed (0-255): </span></td><td>
      <input type="number" name="open_secure_speed" id = "open_secure_speed" min=0 max=255></td></tr>
    <tr><td><span>Close Timeout (ms): </span></td><td>
      <input type="number" name="close_timeout" id = "close_timeout" min=200 max=300000></td></tr>
    <tr><td><span>Close sensor to close (ms): </span></td><td>
      <input type="number" name="close_secure" id = "close_secure" min=200 max=20000></td></tr>
    <tr><td><span>Close secure speed (0-255): </span></td><td>
      <input type="number" name="close_secure_speed" id = "close_secure_speed" min=0 max=255></td></tr>
    <tr><td><span>Default speed (readonly) (0-255): </span></td><td>
      <span id = "default_speed"></span></td></tr>
    <tr><td><span>Submit</span></td><td><input type="submit" target="status"></button><td></tr>
    </form>
  </table>
)=====";

// Serves a single web page, somewhat chunked, back to the client.
void ServePage(ESP8266WebServer &server, const String &title, const char content[]) {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(common_head1);
  server.sendContent(title);
  server.sendContent(common_head2);
  server.sendContent(content);
  server.sendContent(footer);
  server.sendContent("");
  server.client().stop();
}

#endif // ERIC_WINDOW_OPENER_WEB_H
