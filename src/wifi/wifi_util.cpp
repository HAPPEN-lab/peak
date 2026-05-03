#include "wifi_util.hpp"

// Global WiFi server and client objects
WiFiServer server(PORT);
WiFiClient serverClient;
WiFiClient client;

// Global variables for tracking connection state
bool clientConnected = false;
bool wifiConnected = false;

// SERVER FUNCTIONS
void wifi_init_softap()
{
    // Configure and start WiFi in AP mode
    WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, 1); // SSID, password, channel, hidden, max_connection=1

    // Log that the AP has started
    Serial.println("[SERVER] WiFi AP started successfully");
    Serial.printf("[SERVER] SSID: %s, Password: %s\n", WIFI_SSID, WIFI_PASS);
    Serial.printf("[SERVER] Server will be available at IP: %s:%d\n", WiFi.softAPIP().toString().c_str(), PORT);

    // Start TCP server
    server.begin();
    Serial.println("[SERVER] TCP server started");
}

void tcp_server_task()
{
    // Check if we have a client connection
    if (server.hasClient())
    {
        // If we already have a client, disconnect the old one
        if (serverClient && serverClient.connected())
        {
            Serial.println("[SERVER] New client attempting to connect, disconnecting old client");
            serverClient.stop();
        }

        // Accept the new client
        serverClient = server.available();
        if (serverClient)
        {
            Serial.println("[SERVER] Client connected");
            clientConnected = true;
        }
    }

    // If we have a connected client, check for incoming data
    if (clientConnected && serverClient && serverClient.connected())
    {
        if (serverClient.available() >= sizeof(uint32_t))
        {
            Serial.println("[SERVER] Data available from client");

            // Read the float (4 bytes)
            uint8_t data_buf[sizeof(uint32_t)];
            size_t bytes_read = serverClient.readBytes(data_buf, sizeof(uint32_t));

            if (bytes_read == sizeof(uint32_t))
            {
                // Parse the float
                uint32_t netbits;
                memcpy(&netbits, data_buf, sizeof(uint32_t));
                uint32_t hostbits = ntohl(netbits);
                float remaining_balance;
                memcpy(&remaining_balance, &hostbits, sizeof(float));

                Serial.printf("[SERVER] Received remaining balance: %f\n", remaining_balance);

                // Send acknowledgement
                const char *ack = "ACK\n";
                serverClient.write((const uint8_t*)ack, strlen(ack));
                Serial.println("[SERVER] ACK sent");

                // Process the received value
                // TODO: Add processing logic here
                Serial.printf("Remaining balance received: %f\n", remaining_balance);

                // Example: Call AFG algorithm if needed
                // AFG();
            }
            else
            {
                Serial.printf("[SERVER] Incomplete data received: %d bytes\n", bytes_read);
            }
        }
    }
    else if (clientConnected && (!serverClient || !serverClient.connected()))
    {
        // Client disconnected
        Serial.println("[SERVER] Client disconnected");
        clientConnected = false;
        if (serverClient)
        {
            serverClient.stop();
        }
    }
}

// CLIENT FUNCTIONS
void wifi_init_sta()
{
    // Connect to WiFi AP
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.println("[CLIENT] Connecting to WiFi AP...");
    
    // Wait for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.print("[CLIENT] Connected! IP address: ");
        Serial.println(WiFi.localIP());
        Serial.printf("[CLIENT] Ready to connect to server at %s:%d\n", HOST_IP, PORT);
        wifiConnected = true;
    }
    else
    {
        Serial.println();
        Serial.println("[CLIENT] Failed to connect to WiFi AP");
        wifiConnected = false;
    }
}

void tcp_client_task()
{
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[CLIENT] WiFi not connected, reconnecting...");
        wifi_init_sta();
        return;
    }

    // Try to connect to server if not already connected
    if (!client.connected())
    {
        Serial.printf("[CLIENT] Attempting to connect to server at %s:%d\n", HOST_IP, PORT);
        
        IPAddress serverIP;
        if (!serverIP.fromString(HOST_IP))
        {
            Serial.println("[CLIENT] Invalid server IP address");
            delay(5000);
            return;
        }

        if (client.connect(serverIP, PORT))
        {
            Serial.println("[CLIENT] Connected to server!");
        }
        else
        {
            Serial.println("[CLIENT] Connection to server failed, retrying...");
            delay(5000);
            return;
        }
    }

    // If connected, send data
    if (client.connected())
    {
        // Prepare data to send
        float remaining_balance = 67.67; // Placeholder value
        Serial.printf("[CLIENT] Preparing to send remaining balance: %f\n", remaining_balance);

        // Serialize the float into network byte order
        uint8_t out_buffer[sizeof(uint32_t)];
        uint32_t hostbits;
        memcpy(&hostbits, &remaining_balance, sizeof(uint32_t));
        uint32_t netbits = htonl(hostbits);
        memcpy(out_buffer, &netbits, sizeof(uint32_t));

        // Send the data
        size_t bytes_sent = client.write(out_buffer, sizeof(out_buffer));
        if (bytes_sent == sizeof(out_buffer))
        {
            Serial.printf("[CLIENT] Sent remaining balance: %f (%d bytes)\n", remaining_balance, bytes_sent);

            // Wait for ACK
            unsigned long timeout = millis() + 5000; // 5 second timeout
            while (client.available() == 0 && millis() < timeout)
            {
                delay(10);
            }

            if (client.available() > 0)
            {
                String ack = client.readStringUntil('\n');
                Serial.printf("[CLIENT] Received ACK: %s\n", ack.c_str());
            }
            else
            {
                Serial.println("[CLIENT] Timeout waiting for ACK");
            }
        }
        else
        {
            Serial.printf("[CLIENT] Send failed, only %d bytes sent\n", bytes_sent);
        }

        // Wait before sending next update
        delay(WAIT_TIME * 1000);
    }
    else
    {
        Serial.println("[CLIENT] Disconnected from server");
        client.stop();
        delay(5000);
    }
}

// Application entry functions
void run_server_app()
{
    Serial.println("[SERVER] Starting in SERVER mode");
    wifi_init_softap();
}

void run_client_app()
{
    Serial.println("[CLIENT] Starting in CLIENT mode");
    wifi_init_sta();
}
