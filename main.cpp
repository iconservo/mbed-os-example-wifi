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

#define internal        1
#define WIFI_ESP8266    2
#define WIFI_IDW0XX1    3
#define WIFI_ISM43362   4
#define WIFI_WINC1500   5

WiFiInterface *wifi;

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
    static WINC1500Interface winc;
    returmn &winc;
}

#endif


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

void http_demo(NetworkInterface *net)
{
    TCPSocket socket;
    nsapi_error_t response;

    printf("Sending HTTP request to www.arm.com...\n");

    // Open a socket on the network interface, and create a TCP connection to www.arm.com
    response = socket.open(net);
    if(0 != response) {
        printf("socket.open() failed: %d\n", response);
        return;
    }

    response = socket.connect("api.ipify.org", 80);
    if(0 != response) {
        printf("Error connecting: %d\n", response);
        socket.close();
        return;
    }

    // Send a simple http request
    char sbuffer[] = "GET / HTTP/1.1\r\nHost: api.ipify.org\r\nConnection: close\r\n\r\n";
    nsapi_size_t size = strlen(sbuffer);

    // Loop until whole request send
    while(size) {
        response = socket.send(sbuffer+response, size);
        if (response < 0) {
            printf("Error sending data: %d\n", response);
            socket.close();
            return;
        }
        size -= response;
        printf("sent %d [%.*s]\n", response, strstr(sbuffer, "\r\n")-sbuffer, sbuffer);
    }

    // Receieve a simple http response and print out the response line
    char rbuffer[64];
    response = socket.recv(rbuffer, sizeof rbuffer);
    if (response < 0) {
        printf("Error receiving data: %d\n", response);
    } else {
        printf("recv %d [%.*s]\n", response, strstr(rbuffer, "\r\n")-rbuffer, rbuffer);
    }

    // Close the socket to return its memory and bring down the network interface
    socket.close();
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

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    printf("IP: %s\n", wifi->get_ip_address());
    printf("Netmask: %s\n", wifi->get_netmask());
    printf("Gateway: %s\n", wifi->get_gateway());
    printf("RSSI: %d\n\n", wifi->get_rssi());

    http_demo(wifi);

    wifi->disconnect();

#if MBED_CONF_APP_WIFI_SHIELD != internal
    delete wifi;
#endif

    printf("\nDone\n");
}
