#ifndef WIFI_UTIL_HPP
#define WIFI_UTIL_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <cstring>
#include <cstdint>
#include "../afg/micro_afg.hpp"

// Define WIFI constants
#define WIFI_SSID "ESP32-AP"
#define WIFI_PASS "12345678"
#define HOST_IP "192.168.4.1"
#define PORT 3333

// Set the amount of time the client should wait before sending new data
#define WAIT_TIME 30 // in seconds

// Define structs
struct client_params
{
    float benefit_per_unit_cost[NUMBER_OF_LOADS][NUMBER_OF_DAYS];
    float demand_avg_power_W[NUMBER_OF_LOADS][NUMBER_OF_DAYS];
    const char *TAG;
};

// Define functions
void wifi_init_softap();
void tcp_server_task();
void wifi_init_sta();
void tcp_client_task();

// Application entry functions
void run_server_app();
void run_client_app();

#endif // WIFI_UTIL_HPP
