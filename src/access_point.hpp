#pragma once

#define ICACHE_RODATA_ATTR  __attribute__((section(".irom.text")))
#define PROGMEM   ICACHE_RODATA_ATTR

static const char setup_webpage[] PROGMEM = R"rawliteral(
<html lang="en">
<head>
    <meta charset="utf-8">
    <title>Node WiFi Setup</title>
</head>
<body>
    <form>
        <fieldset>
            <div>
                <label for="ssid">WiFi Name</label>
                <input value="" id="ssid" placeholder="SSID">
            </div>
            <div>
                <label for="password">WiFi Password</label>
                <input type="password" value="" id="password" placeholder="PASSWORD">
            </div>
            <div>
                <label for="ip">Server IP Address</label>
                <input value="" id="ip" placeholder="IP Address">
            </div>
            <div>
                <label for="cert">Node certificate</label>
                <input type="file" value="" id="cert">
            </div>
            <div>
                <button class="primary" type="button" onclick="send()">Save</button>
            </div>
        </fieldset>
    </form>
    <p id="status"></p>
</body>
<script>
    function send() {
        var files = document.getElementById('cert').files;
        if (!files.length) {
            alert('Please select a file!');
            return;
        }

        var file = files[0];
        var start = 0;
        var stop = file.size - 1;

        var reader = new FileReader();

        // If we use onloadend, we need to check the readyState.
        reader.onloadend = function(evt) {
            if (evt.target.readyState == FileReader.DONE) { // DONE == 2

                const ssid = document.getElementById("ssid").value;
                const password = document.getElementById("password").value;
                const ip = document.getElementById("ip").value;

                const data = {ssid: ssid, password: password, cert: evt.target.result, ip: ip}

                var xhr = new XMLHttpRequest();
                var url = "/save";
                xhr.onreadystatechange = function() {
                    if (this.readyState == 4 && this.status == 200) {
                        const pJson = JSON.parse(xhr.responseText)
                        alert(pJson.status)
                    }
                };
                xhr.open("POST", url, true);
                xhr.send(JSON.stringify(data));
            }
        };

        var blob = file.slice(start, stop + 1);
        reader.readAsBinaryString(blob);
    }
</script>
</html>
)rawliteral";

void start_setup_server();
bool start_setup_ap();
void handle_setup_post();
void handle_client();
