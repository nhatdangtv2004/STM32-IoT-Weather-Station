#include <Arduino.h>
#include <WiFi.h>
#include <MQTT.h>


/* =========================================================
   WIFI CONFIG
   ========================================================= */

#define WIFI_SSID       "..."
#define WIFI_PASSWORD   "..."
// https://www.hivemq.com/demos/websocket-client/

/* =========================================================
   MQTT CONFIG
   Host: broker.hivemq.com
Port: 8884

Username: để trống
Password: để trống

SSL: bật
Clean Session: bật cũng được
Keep Alive: 60

QoS: 0

Topic:
hcmute/weatherstation/weather-C022C8D8CBB0/#

node-red trong cmd
   ========================================================= */

/*
 * Public broker chi dung de test.
 */

#define MQTT_BROKER     "broker.hivemq.com"
#define MQTT_PORT       1883


WiFiClient mqttNetwork;

MQTTClient mqttClient(256);


/* =========================================================
   UART STM32 -> ESP32
   ========================================================= */

#define WEATHER_RX_PIN     16
#define WEATHER_TX_PIN     17

#define WEATHER_BAUD       115200

#define PACKET_SIZE        16

#define HEADER_1           0xAA
#define HEADER_2           0x55


HardwareSerial WeatherSerial(2);


/* =========================================================
   TIMING
   ========================================================= */

#define WIFI_RETRY_INTERVAL    10000UL
#define MQTT_RETRY_INTERVAL     5000UL


unsigned long last_wifi_retry = 0;
unsigned long last_mqtt_retry = 0;


/* =========================================================
   MQTT NAMES / TOPICS
   ========================================================= */

char mqtt_client_id[40];

char mqtt_topic_root[80];

char topic_temperature[100];
char topic_humidity[100];
char topic_pressure[100];
char topic_light[100];
char topic_rain_raw[100];
char topic_rain_state[100];
char topic_status[100];


/* =========================================================
   BUILD UNIQUE MQTT TOPICS
   ========================================================= */

void BuildMQTTTopics()
{
    uint64_t chip_id =
        ESP.getEfuseMac();


    snprintf(
        mqtt_client_id,
        sizeof(mqtt_client_id),
        "weather-%04X%08X",
        (uint16_t)(chip_id >> 32),
        (uint32_t)chip_id
    );


    snprintf(
        mqtt_topic_root,
        sizeof(mqtt_topic_root),
        "hcmute/weatherstation/%s",
        mqtt_client_id
    );


    snprintf(
        topic_temperature,
        sizeof(topic_temperature),
        "%s/temperature",
        mqtt_topic_root
    );


    snprintf(
        topic_humidity,
        sizeof(topic_humidity),
        "%s/humidity",
        mqtt_topic_root
    );


    snprintf(
        topic_pressure,
        sizeof(topic_pressure),
        "%s/pressure",
        mqtt_topic_root
    );


    snprintf(
        topic_light,
        sizeof(topic_light),
        "%s/light",
        mqtt_topic_root
    );


    snprintf(
        topic_rain_raw,
        sizeof(topic_rain_raw),
        "%s/rain/raw",
        mqtt_topic_root
    );


    snprintf(
        topic_rain_state,
        sizeof(topic_rain_state),
        "%s/rain/state",
        mqtt_topic_root
    );


    snprintf(
        topic_status,
        sizeof(topic_status),
        "%s/status",
        mqtt_topic_root
    );
}


/* =========================================================
   LITTLE ENDIAN HELPERS
   ========================================================= */

uint16_t GetU16LE(
    const uint8_t *data
)
{
    return
        ((uint16_t)data[0])
        |
        ((uint16_t)data[1] << 8);
}


int16_t GetI16LE(
    const uint8_t *data
)
{
    return
        (int16_t)GetU16LE(
            data
        );
}


uint32_t GetU32LE(
    const uint8_t *data
)
{
    return
        ((uint32_t)data[0])
        |
        ((uint32_t)data[1] << 8)
        |
        ((uint32_t)data[2] << 16)
        |
        ((uint32_t)data[3] << 24);
}


/* =========================================================
   XOR CHECKSUM
   ========================================================= */

uint8_t CalculateChecksum(
    const uint8_t *data,
    uint8_t length
)
{
    uint8_t checksum = 0;


    for (uint8_t i = 0;
         i < length;
         i++)
    {
        checksum ^=
            data[i];
    }


    return checksum;
}


/* =========================================================
   RAIN STATE -> TEXT
   ========================================================= */

const char *RainStateText(
    uint8_t state
)
{
    switch (state)
    {
        case 0:
            return "DRY";

        case 1:
            return "LOW";

        case 2:
            return "HEAVY";

        default:
            return "UNKNOWN";
    }
}


/* =========================================================
   FORMAT SIGNED FIXED-POINT x100
   ========================================================= */

void FormatSignedFixed2(
    int16_t value,
    char *buffer,
    size_t buffer_size
)
{
    int32_t number =
        value;


    if (number < 0)
    {
        number =
            -number;


        snprintf(
            buffer,
            buffer_size,
            "-%ld.%02ld",
            (long)(number / 100),
            (long)(number % 100)
        );
    }

    else
    {
        snprintf(
            buffer,
            buffer_size,
            "%ld.%02ld",
            (long)(number / 100),
            (long)(number % 100)
        );
    }
}


/* =========================================================
   FORMAT UNSIGNED FIXED-POINT x100
   ========================================================= */

void FormatUnsignedFixed2(
    uint32_t value,
    char *buffer,
    size_t buffer_size
)
{
    snprintf(
        buffer,
        buffer_size,
        "%lu.%02lu",
        (unsigned long)(
            value / 100
        ),
        (unsigned long)(
            value % 100
        )
    );
}


/* =========================================================
   WIFI CONNECT
   ========================================================= */

void ConnectWiFi()
{
    Serial.println();

    Serial.print(
        "Connecting to WiFi: "
    );

    Serial.println(
        WIFI_SSID
    );


    WiFi.mode(
        WIFI_STA
    );


    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );


    unsigned long start_time =
        millis();


    while (
        WiFi.status() != WL_CONNECTED &&
        (millis() - start_time) < 15000UL
    )
    {
        delay(500);

        Serial.print(".");
    }


    Serial.println();


    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {
        Serial.println(
            "WiFi CONNECTED"
        );


        Serial.print(
            "IP address: "
        );

        Serial.println(
            WiFi.localIP()
        );


        Serial.print(
            "RSSI: "
        );

        Serial.print(
            WiFi.RSSI()
        );

        Serial.println(
            " dBm"
        );
    }

    else
    {
        Serial.println(
            "WiFi connection FAILED"
        );
    }
}


/* =========================================================
   WIFI SERVICE
   ========================================================= */

void ServiceWiFi()
{
    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {
        return;
    }


    if (
        millis() -
        last_wifi_retry >=
        WIFI_RETRY_INTERVAL
    )
    {
        last_wifi_retry =
            millis();


        Serial.println(
            "WiFi reconnecting..."
        );


        WiFi.disconnect();


        WiFi.begin(
            WIFI_SSID,
            WIFI_PASSWORD
        );
    }
}


/* =========================================================
   MQTT CONNECT
   ========================================================= */

void ServiceMQTT()
{
    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        return;
    }


    if (
        mqttClient.connected()
    )
    {
        mqttClient.loop();

        return;
    }


    if (
        millis() -
        last_mqtt_retry <
        MQTT_RETRY_INTERVAL
    )
    {
        return;
    }


    last_mqtt_retry =
        millis();


    Serial.print(
        "Connecting MQTT..."
    );


    if (
        mqttClient.connect(
            mqtt_client_id
        )
    )
    {
        Serial.println(
            "CONNECTED"
        );


        mqttClient.publish(
            topic_status,
            "ONLINE",
            true,
            0
        );


        Serial.print(
            "MQTT topic root: "
        );

        Serial.println(
            mqtt_topic_root
        );
    }

    else
    {
        Serial.println(
            "FAILED"
        );


        Serial.print(
            "MQTT error: "
        );

        Serial.println(
            (int)mqttClient.lastError()
        );


        Serial.print(
            "MQTT return code: "
        );

        Serial.println(
            (int)mqttClient.returnCode()
        );
    }
}


/* =========================================================
   MQTT PUBLISH WEATHER
   ========================================================= */

void PublishWeatherMQTT(
    int16_t temperature_x100,
    uint16_t humidity_x100,
    uint32_t pressure_x100,
    uint16_t light_lux,
    uint16_t rain_raw,
    uint8_t rain_state
)
{
    if (
        !mqttClient.connected()
    )
    {
        return;
    }


    char payload[32];


    bool publish_ok =
        true;


    /* =============================================
       TEMPERATURE
       ============================================= */

    FormatSignedFixed2(
        temperature_x100,
        payload,
        sizeof(payload)
    );


    publish_ok &=
        mqttClient.publish(
            topic_temperature,
            payload,
            true,
            0
        );


    /* =============================================
       HUMIDITY
       ============================================= */

    FormatUnsignedFixed2(
        humidity_x100,
        payload,
        sizeof(payload)
    );


    publish_ok &=
        mqttClient.publish(
            topic_humidity,
            payload,
            true,
            0
        );


    /* =============================================
       PRESSURE
       ============================================= */

    FormatUnsignedFixed2(
        pressure_x100,
        payload,
        sizeof(payload)
    );


    publish_ok &=
        mqttClient.publish(
            topic_pressure,
            payload,
            true,
            0
        );


    /* =============================================
       LIGHT
       ============================================= */

    snprintf(
        payload,
        sizeof(payload),
        "%u",
        light_lux
    );


    publish_ok &=
        mqttClient.publish(
            topic_light,
            payload,
            true,
            0
        );


    /* =============================================
       RAIN RAW
       ============================================= */

    snprintf(
        payload,
        sizeof(payload),
        "%u",
        rain_raw
    );


    publish_ok &=
        mqttClient.publish(
            topic_rain_raw,
            payload,
            true,
            0
        );


    /* =============================================
       RAIN STATE
       ============================================= */

    publish_ok &=
        mqttClient.publish(
            topic_rain_state,
            RainStateText(
                rain_state
            ),
            true,
            0
        );


    if (
        publish_ok
    )
    {
        Serial.println(
            "MQTT        : PUBLISHED"
        );
    }

    else
    {
        Serial.println(
            "MQTT        : PUBLISH ERROR"
        );
    }
}


/* =========================================================
   PROCESS WEATHER PACKET
   ========================================================= */

void ProcessWeatherPacket(
    const uint8_t *packet
)
{
    int16_t temperature_x100;

    uint16_t humidity_x100;

    uint32_t pressure_x100;

    uint16_t light_lux;

    uint16_t rain_raw;

    uint8_t rain_state;


    /* =============================================
       DECODE STM32 PACKET
       ============================================= */

    temperature_x100 =
        GetI16LE(
            &packet[2]
        );


    humidity_x100 =
        GetU16LE(
            &packet[4]
        );


    pressure_x100 =
        GetU32LE(
            &packet[6]
        );


    light_lux =
        GetU16LE(
            &packet[10]
        );


    rain_raw =
        GetU16LE(
            &packet[12]
        );


    rain_state =
        packet[14];


    /* =============================================
       SERIAL MONITOR
       ============================================= */

    Serial.println();

    Serial.println(
        "========== WEATHER STATION =========="
    );


    Serial.printf(
        "Temperature : %.2f C\n",
        (float)temperature_x100 /
        100.0f
    );


    Serial.printf(
        "Humidity    : %.2f %%\n",
        (float)humidity_x100 /
        100.0f
    );


    Serial.printf(
        "Pressure    : %.2f hPa\n",
        (float)pressure_x100 /
        100.0f
    );


    Serial.printf(
        "Light       : %u lux\n",
        light_lux
    );


    Serial.printf(
        "Rain ADC    : %u\n",
        rain_raw
    );


    Serial.printf(
        "Rain State  : %s\n",
        RainStateText(
            rain_state
        )
    );


    if (
        WiFi.status() ==
        WL_CONNECTED
    )
    {
        Serial.print(
            "WiFi        : CONNECTED | "
        );

        Serial.println(
            WiFi.localIP()
        );
    }

    else
    {
        Serial.println(
            "WiFi        : DISCONNECTED"
        );
    }


    if (
        mqttClient.connected()
    )
    {
        Serial.println(
            "MQTT        : CONNECTED"
        );
    }

    else
    {
        Serial.println(
            "MQTT        : DISCONNECTED"
        );
    }


    /* =============================================
       MQTT PUBLISH
       ============================================= */

    PublishWeatherMQTT(
        temperature_x100,
        humidity_x100,
        pressure_x100,
        light_lux,
        rain_raw,
        rain_state
    );


    Serial.println(
        "====================================="
    );
}


/* =========================================================
   SETUP
   ========================================================= */

void setup()
{
    /* =============================================
       USB SERIAL
       ============================================= */

    Serial.begin(
        115200
    );


    delay(
        1000
    );


    Serial.println();

    Serial.println(
        "ESP32 Weather Gateway"
    );


    /* =============================================
       BUILD MQTT TOPICS
       ============================================= */

    BuildMQTTTopics();


    Serial.print(
        "MQTT Client ID: "
    );

    Serial.println(
        mqtt_client_id
    );


    /* =============================================
       UART2 -> STM32
       ============================================= */

    WeatherSerial.begin(
        WEATHER_BAUD,
        SERIAL_8N1,
        WEATHER_RX_PIN,
        WEATHER_TX_PIN
    );


    Serial.println(
        "UART2 ready."
    );


    Serial.println(
        "Waiting for STM32 data..."
    );


    /* =============================================
       WIFI
       ============================================= */

    ConnectWiFi();


    /* =============================================
       MQTT
       ============================================= */

    mqttClient.begin(
        MQTT_BROKER,
        MQTT_PORT,
        mqttNetwork
    );


    mqttClient.setWill(
        topic_status,
        "OFFLINE",
        true,
        0
    );


    /*
     * Cho phep connect ngay lan dau.
     */

    last_mqtt_retry =
        millis() -
        MQTT_RETRY_INTERVAL;
}


/* =========================================================
   LOOP
   ========================================================= */

void loop()
{
    static uint8_t frame[
        PACKET_SIZE
    ];


    static uint8_t frame_index =
        0;


    /* =============================================
       NETWORK SERVICES
       ============================================= */

    ServiceWiFi();

    ServiceMQTT();


    /* =============================================
       RECEIVE STM32 UART
       ============================================= */

    while (
        WeatherSerial.available() > 0
    )
    {
        uint8_t byte_received =
            (uint8_t)
            WeatherSerial.read();


        /* =========================================
           HEADER 0xAA
           ========================================= */

        if (
            frame_index == 0
        )
        {
            if (
                byte_received ==
                HEADER_1
            )
            {
                frame[0] =
                    byte_received;

                frame_index =
                    1;
            }


            continue;
        }


        /* =========================================
           HEADER 0x55
           ========================================= */

        if (
            frame_index == 1
        )
        {
            if (
                byte_received ==
                HEADER_2
            )
            {
                frame[1] =
                    byte_received;

                frame_index =
                    2;
            }

            else if (
                byte_received ==
                HEADER_1
            )
            {
                frame[0] =
                    HEADER_1;

                frame_index =
                    1;
            }

            else
            {
                frame_index =
                    0;
            }


            continue;
        }


        /* =========================================
           PACKET BODY
           ========================================= */

        frame[
            frame_index
        ] =
            byte_received;


        frame_index++;


        /* =========================================
           COMPLETE PACKET
           ========================================= */

        if (
            frame_index ==
            PACKET_SIZE
        )
        {
            uint8_t checksum;


            checksum =
                CalculateChecksum(
                    frame,
                    15
                );


            if (
                checksum ==
                frame[15]
            )
            {
                ProcessWeatherPacket(
                    frame
                );
            }

            else
            {
                Serial.printf(
                    "Checksum ERROR: RX=%02X CALC=%02X\n",
                    frame[15],
                    checksum
                );
            }


            frame_index =
                0;
        }
    }
}