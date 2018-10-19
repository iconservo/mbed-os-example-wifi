/* WiFi Example
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "TCPSocket.h"

#include "wifi-winc1500/WINC1500TLSSocket.h"


#include "wifi-winc1500/mbed_bsp/bsp_mbed.h"

#include "mbed-http/source/http_request.h"


extern "C"
{
#include "wifi-winc1500/winc1500/host_drv/driver/include/m2m_wifi.h"
#include "wifi-winc1500/winc1500/host_drv/driver/source/m2m_hif.h"
#include "wifi-winc1500/winc1500/host_drv/driver/include/m2m_types.h"


}


#define internal        1
#define WIFI_ESP8266    2
#define WIFI_IDW0XX1    3
#define WIFI_ISM43362   4
#define WIFI_WINC1500   5

WiFiInterface *wifi;

//static DigitalOut lp_en(LOW_POWER_ENABLE);


/** Wi-Fi Settings */
#define MAIN_WLAN_SSID        "CiklumBiowaveLabs" /* < Destination SSID */
#define MAIN_WLAN_AUTH        M2M_WIFI_SEC_WPA_PSK /* < Security manner */
#define MAIN_WLAN_PSK         "Biowave_is_the_best" /* < Password for Destination SSID */

#define MAIN_HTTP_FILE_URL   			"https://home.t.myblossom.com/api/heartbeat/"//"https://httpbin.org/get"//"https://www.google.com"//"httpbin.org/get"//"https://www.myblossom.com/"//"http://home.s.myblossom.com/"
#define USE_TLS							0//1
#define PORT							80//443

#if MBED_CONF_APP_WIFI_SHIELD == WIFI_ESP8266

#include "ESP8266Interface.h"

WiFiInterface *WiFiInterface::get_default_instance() {
    static ESP8266Interface esp(MBED_CONF_APP_WIFI_TX, MBED_CONF_APP_WIFI_RX);
    return &esp;
}

#elif MBED_CONF_APP_WIFI_SHIELD == WIFI_ISM43362

#include "ISM43362Interface.h"

WiFiInterface *WiFiInterface::get_default_instance() {
    static ISM43362Interface ism;
    return &ism;
}

#elif MBED_CONF_APP_WIFI_SHIELD == WIFI_IDW0XX1

#include "SpwfSAInterface.h"

WiFiInterface *WiFiInterface::get_default_instance() {
    static SpwfSAInterface spwf(MBED_CONF_APP_WIFI_TX, MBED_CONF_APP_WIFI_RX);
    return &spwf;
}

#elif MBED_CONF_APP_WIFI_SHIELD == WIFI_WINC1500

#include "WINC1500Interface.h"

WiFiInterface *WiFiInterface::get_default_instance() {

	return &WINC1500Interface::getInstance();
}

#endif


FileHandle* mbed::mbed_override_console(int fd) {
    static UARTSerial s(USBTX, USBRX, 115200);
    return &s;
}

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan DEMO\n");

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

void dump_response(HttpResponse* res) {
    printf("Status: %d - %s\n", res->get_status_code(), res->get_status_message().c_str());

    printf("Headers:\n");
    for (size_t ix = 0; ix < res->get_headers_length(); ix++) {
        printf("\t%s: %s\n", res->get_headers_fields()[ix]->c_str(), res->get_headers_values()[ix]->c_str());
    }
    printf("\nBody (%d bytes):\n\n%s\n", res->get_body_length(), res->get_body_as_string().c_str());
}


int https_demo(NetworkInterface *net)
{
    // Create a TCP socket
    printf("\n----- Setting up TCP connection -----\n");

    //  TCPSocket* socket = new TCPSocket();
    WINC1500TLSSocket* socket = new WINC1500TLSSocket();


    nsapi_error_t open_result = socket->open((WINC1500Interface *)wifi);
    if (open_result != 0) {
        printf("Opening TCPSocket failed... %d\n", open_result);
        return 1;
    }

    nsapi_error_t connect_result = socket->connect("home.t.myblossom.com", 443);
    if (connect_result != 0) {
        printf("Connecting over TCPSocket failed... %d\n", connect_result);
        return 1;
    }

    printf("Connected over TCP to home.t.myblossom.com/api/heartbeat/:443\n");

    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);
    printf("Current heap: %lu\r\n", heap_stats.current_size);
    printf("Max heap size: %lu\r\n", heap_stats.max_size);

    // Do a GET re_Static_assertquest to https://home.t.myblossom.com/api/heartbeat/
    {
        HttpRequest* get_req = new HttpRequest(socket, HTTP_GET, "https://home.t.myblossom.com/api/heartbeat/");

        // By default the body is automatically parsed and stored in a string, this is memory heavy.
        // To receive chunked response, pass in a callback as third parameter to 'send'.
        HttpResponse* get_res = get_req->send();
        if (!get_res) {
            printf("HttpRequest failed (error code %d)\n", get_req->get_error());
            return 1;
        }

        printf("\n----- HTTP GET response -----\n");
        dump_response(get_res);

        delete get_req;
    }

    // POST request to httpbin.org
    {
        HttpRequest* post_req = new HttpRequest(socket, HTTP_POST, "https://home.t.myblossom.com/api/heartbeat/");
        post_req->set_header("Content-Type", "application/json");

        const char body[] = "{\"hello\":\"world\"}";

        HttpResponse* post_res = post_req->send(body, strlen(body));
        if (!post_res) {
            printf("HttpRequest failed (error code %d)\n", post_req->get_error());
            return 1;
        }

        printf("\n----- HTTP POST response -----\n");
        dump_response(post_res);

        delete post_req;
    }

    delete socket;
}


int main()
{
    int count = 0;

    printf("WiFi example\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    count = scan_demo(wifi);
    if (count == 0) {
        printf("No WIFI APNs found - can't continue further.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MAIN_WLAN_SSID);

    sint8 ret = wifi->connect(MAIN_WLAN_SSID, MAIN_WLAN_PSK, NSAPI_SECURITY_WPA_WPA2, M2M_WIFI_CH_ALL);

    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }
    else
    {
        printf("Success\n\n");

    }

    printf("HTTPS demo with Blossom server\n\n");

    https_demo(wifi);

    wifi->disconnect();

    printf("\nDone\n");

    wait(osWaitForever);

}
