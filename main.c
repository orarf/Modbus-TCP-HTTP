#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <conio.h>
#include <curl/curl.h>
#include "ModbusLib/src/cModbus.h"

// ฟังก์ชันแปลงค่า 2 รีจิสเตอร์เป็น float แบบ little-endian
float modbusToFloat_LE(uint16_t hi, uint16_t lo) {
    union { uint32_t i; float f; } u;
    u.i = ((uint32_t)hi << 16) | lo;
    return u.f;
}

// ฟังก์ชันส่ง Telemetry ไป ThingsBoard
void sendTelemetry(const char* token, const char* server, const char* name, float value) {
    CURL *curl = curl_easy_init();
    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "https://%s/api/v1/%s/telemetry", server, token); // ใช้ https

        char payload[128];
        snprintf(payload, sizeof(payload), "{\"%s\": %.2f}", name, value);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

        // ถ้าทดสอบภายใน แล้วยังเจอ SSL error ใส่ข้างล่างนี้ (ไม่ปลอดภัย แต่แก้ทดสอบได้)
        // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

int main() {
    TcpSettings settings;
    settings.host = "192.168.100.28";   // IP ของ Power Meter/Modbus
    settings.port = STANDARD_TCP_PORT;
    settings.timeout = 3000;

    const uint8_t unit = 1;
    cModbusClient client = cCliCreate(unit, TCP, &settings, 1);

    int addresses[] = {2999, 3001, 3003, 3005, 3007, 3009};
    const char* names[] = {"Current A", "Current B", "Current C", "Current N", "Current G", "Current AVG."};
    int count = sizeof(addresses) / sizeof(addresses[0]);
    uint16_t regs[2];
    char ch = 0;

    // --- แก้ตรงนี้ ---
    const char* tb_server = "thingsboard.tricommtha.com";           // IP/Domain ของ ThingsBoard server
    const char* access_token = "Oalil8rRoURFDLs1IW8V";  // ใส่ access token ของ device

    printf("กด 'q' เพื่อออกจากโปรแกรม\n");

    curl_global_init(CURL_GLOBAL_ALL); // เตรียม libcurl

    while (1) {
        for (int i = 0; i < count; ++i) {
            StatusCode status = cReadHoldingRegisters(client, addresses[i], 2, regs);
            printf("%s [%d]: ", names[i], addresses[i]);
            if (StatusIsGood(status)) {
                float value = modbusToFloat_LE(regs[0], regs[1]);
                printf("%.2f\n", value);
                sendTelemetry(access_token, tb_server, names[i], value);
            } else {
                printf("Error (code %d)\n", status);
            }
        }
        printf("---\n");

        Sleep(2000); // หน่วง 2 วินาทีก่อนอ่านรอบใหม่
        if (_kbhit()) {
            ch = _getch();
            if (ch == 'q' || ch == 'Q')
                break;
        }
    }

    curl_global_cleanup();
    cCliDelete(client);
    return 0;
}
