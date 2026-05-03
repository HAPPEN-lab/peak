#include <Arduino.h>
#include "afg/afg.hpp"
#include <SPI.h>

// Define data pins
#define SD_MISO 16
#define SD_MOSI 15
#define SD_SCK 17
#define SD_CS 7

SPIClass spi = SPIClass(FSPI);

// Put thresholds and enable durations in DRAM
static float threshold[NUMBER_OF_LOADS][NUMBER_OF_DAYS];
static float enable_duration_max[NUMBER_OF_LOADS][NUMBER_OF_DAYS];

void setup()
{
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000); // Give serial time to initialize

    spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, spi, 4000000))
    {
        Serial.println("SD Card Mount Failed.");
        return;
    }

    Serial.println("\n\n=================================");
    Serial.println("ESP32 Prepaid Energy Manager");
    Serial.println("=================================\n");
    Serial.println("Running AFG algorithm");

    // Define test parameters
    int start_day = 1;
    int load_priority_order[NUMBER_OF_LOADS] = {2, 4, 3, 1}; // Priority order (lower number = higher priority)

    // // Start tracking memory and time
    // uint32_t start_time = micros();
    // uint32_t start_free_heap = ESP.getFreeHeap();
    // UBaseType_t start_stack_hwm = uxTaskGetStackHighWaterMark(NULL);

    // Run the AFG algorithm
    AFG(start_day, load_priority_order, threshold, enable_duration_max);

    // // Stop tracking and calculate metrics
    // uint32_t end_time = micros();
    // uint32_t end_min_free_heap = ESP.getMinFreeHeap();
    // UBaseType_t end_stack_hwm = uxTaskGetStackHighWaterMark(NULL);

    // float elapsed_seconds = (end_time - start_time) / 1000000.0;
    
    // // ESP.getMinFreeHeap() gives the lowest free heap ever seen since boot.
    // // By comparing the free heap right before to the minimum free heap after,
    // // we can calculate the peak dynamic memory (heap) allocated during the run.
    // uint32_t peak_heap_used = start_free_heap > end_min_free_heap ? start_free_heap - end_min_free_heap : 0;

    // // uxTaskGetStackHighWaterMark returns the minimum remaining stack space (in bytes for ESP32).
    // // The difference gives the peak stack usage of the function call.
    // uint32_t peak_stack_used = start_stack_hwm > end_stack_hwm ? start_stack_hwm - end_stack_hwm : 0;

    // Serial.println("\n--- Performance Metrics ---");
    // Serial.print("Total time used: ");
    // Serial.print(elapsed_seconds, 6);
    // Serial.println(" seconds");
    // Serial.print("Peak heap RAM usage:  ");
    // Serial.print(peak_heap_used);
    // Serial.println(" bytes");
    // Serial.print("Peak stack RAM usage: ");
    // Serial.print(peak_stack_used);
    // Serial.println(" bytes");
    // Serial.print("Total peak dynamic RAM: ");
    // Serial.print(peak_heap_used + peak_stack_used);
    // Serial.println(" bytes");

    Serial.println("\nAFG algorithm completed. Program will now halt.");
    Serial.flush();
}

void loop()
{
    // Do nothing - program runs once in setup
    delay(1000);
}
