#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <SD.h>
#include <FreeRTOS.h>

struct DNSHeader {
    uint16_t ID;
    uint16_t Flags;
    uint16_t QCount;
    uint16_t AnsCount;
    uint16_t AuthCount;
    uint16_t AddCount;
} __attribute__((packed));

const int DNS_PORT = 53;
const IPAddress UPSTREAM_DNS(9, 9, 9, 9); // Quad9 DNS for good security
const int CHIP_SELECT = 12;
const char* LOG_FILE_BASE_NAME = "dnslog";
const char* LOG_FILE_EXTENSION = ".txt";
const int MAX_LOG_FILES = 5;
const size_t MAX_LOG_FILE_SIZE = 1024 * 10;

struct DomainMapping {
    const char* domainName;
    IPAddress ipAddress;
};

DomainMapping domainMappings[] = {
    {"example.com", IPAddress(192, 168, 1, 1)},
    {"example2.com", IPAddress(192, 168, 1, 2)},
};

const int DOMAIN_MAPPINGS_COUNT = sizeof(domainMappings) / sizeof(DomainMapping);

struct DnsCacheEntry {
    char domainName[100];
    IPAddress ipAddress;
};

const int DNS_CACHE_SIZE = 10;
DnsCacheEntry dnsCache[DNS_CACHE_SIZE];

WiFiUDP udp;
TaskHandle_t dnsTaskHandle = NULL;
volatile bool serverRunning = false;

void DNSserverTask(void *pvParameters);
bool startDnsServer();
void stopDnsServer();
void processDnsQueries();
void setupWifiWithSmartConfig();
void DNSsetup();
void handleDnsQuery(const byte* query, int querySize, WiFiUDP& udp);
void setupDNSResponseHeader(byte* response, int* responseSize, const byte* query);
void appendDNSAnswer(byte* response, int* responseSize, IPAddress ip);
void setResponseCodeNXDOMAIN(byte* response, int* responseSize);
bool forwardQueryToUpstream(const byte* query, int querySize, IPAddress& responseIp);
bool extractIPFromResponse(const byte* response, int packetSize, IPAddress& ip);
bool lookupCache(const char* domain, IPAddress& ip);
bool extractDomainNameFromQuery(const byte* query, int querySize, char* queryDomain);
bool findDomainIP(const char* domain, IPAddress& outIpAddress);
void addCacheEntry(const char* domain, const IPAddress& ip);
void logToSDCard(const char* logMessage);
void rotateLogs();
bool findInLogs(const char* domain, IPAddress& ipAddress);
void sendDnsResponse(WiFiUDP& udp, const byte* response, int responseSize);

void DNSsetup() {
    Serial.begin(115200);
    if (!SD.begin(CHIP_SELECT)) {
        Serial.println("Card Mount Failed");
        return;
    }
    Serial.println("SD Card initialized.");}

bool startDnsServer() {
    DNSsetup();
    if (dnsTaskHandle == NULL) {
        serverRunning = true;
        xTaskCreate(DNSserverTask, "DNS Server Task", 4096, NULL, 1, &dnsTaskHandle);
        Serial.println("DNS Server task created and started.");
    }
    return WiFi.isConnected() && serverRunning;
}

void stopDnsServer() {
    if (dnsTaskHandle != NULL) {
        serverRunning = false;
        vTaskDelete(dnsTaskHandle);
        dnsTaskHandle = NULL;
        udp.stop();
        Serial.println("DNS Server stopped and task deleted.");
    }
}

void DNSserverTask(void *pvParameters) {
    udp.begin(DNS_PORT);
    while (serverRunning) {
        processDnsQueries();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

void processDnsQueries() {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        byte query[512];
        udp.read(query, packetSize);
        handleDnsQuery(query, packetSize, udp);
    }
}

void handleDnsQuery(const byte* query, int querySize, WiFiUDP& udp) {
    IPAddress responseIp;
    byte response[512];
    int responseSize = 0;
    char queryDomain[256];

    if (extractDomainNameFromQuery(query, querySize, queryDomain)) {
        if (!lookupCache(queryDomain, responseIp) && !findInLogs(queryDomain, responseIp) && !findDomainIP(queryDomain, responseIp)) {
            if (!forwardQueryToUpstream(query, querySize, responseIp)) {
                setResponseCodeNXDOMAIN(response, &responseSize);
            } else {
                addCacheEntry(queryDomain, responseIp);
            }
        }

        if (responseSize == 0) {
            setupDNSResponseHeader(response, &responseSize, query);
            appendDNSAnswer(response, &responseSize, responseIp);
        }

        sendDnsResponse(udp, response, responseSize);
    }
}

void setupDNSResponseHeader(byte* response, int* responseSize, const byte* query) {
    DNSHeader* dnsHeader = (DNSHeader*)response;
    DNSHeader* queryHeader = (DNSHeader*)query;

    dnsHeader->ID = queryHeader->ID;
    dnsHeader->Flags = htons(0x8400);
    dnsHeader->QCount = htons(1);
    dnsHeader->AnsCount = htons(1);
    dnsHeader->AuthCount = 0;
    dnsHeader->AddCount = 0;

    *responseSize = sizeof(DNSHeader);
}

void appendDNSAnswer(byte* response, int* responseSize, IPAddress ip) {
    int index = *responseSize;

    response[index++] = 0xC0;
    response[index++] = 0x0C;

    response[index++] = 0x00;
    response[index++] = 0x01;

    response[index++] = 0x00;
    response[index++] = 0x01;

    uint32_t ttl = htonl(60);
    memcpy(response + index, &ttl, sizeof(ttl));
    index += sizeof(ttl);

    response[index++] = 0x00;
    response[index++] = 0x04;

    for (int i = 0; i < 4; i++) {
        response[index++] = ip[i];
    }

    *responseSize = index;
}

void setResponseCodeNXDOMAIN(byte* response, int* responseSize) {
    DNSHeader* dnsHeader = (DNSHeader*)response;
    dnsHeader->Flags = htons(0x8403);
    *responseSize = sizeof(DNSHeader);
}

bool forwardQueryToUpstream(const byte* query, int querySize, IPAddress& responseIp) {
    WiFiUDP udpUpstream;
    udpUpstream.beginPacket(UPSTREAM_DNS, 53);
    udpUpstream.write(query, querySize);
    udpUpstream.endPacket();

    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
        int packetSize = udpUpstream.parsePacket();
        if (packetSize > 0) {
            byte response[512];
            udpUpstream.read(response, packetSize);
            if (extractIPFromResponse(response, packetSize, responseIp)) {
                return true;
            }
        }
    }
    return false;
}

bool extractIPFromResponse(const byte* response, int packetSize, IPAddress& ip) {
    if (packetSize > sizeof(DNSHeader)) {
        int rDataStart = sizeof(DNSHeader) + 12;
        if (packetSize > rDataStart + 10) {
            ip = IPAddress(response[rDataStart + 10], response[rDataStart + 11], response[rDataStart + 12], response[rDataStart + 13]);
            return true;
        }
    }
    return false;
}

bool lookupCache(const char* domain, IPAddress& ip) {
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (strcasecmp(dnsCache[i].domainName, domain) == 0) {
            ip = dnsCache[i].ipAddress;
            return true;
        }
    }
    return false;
}

bool extractDomainNameFromQuery(const byte* query, int querySize, char* queryDomain) {
    int index = sizeof(DNSHeader);
    int j = 0;
    while (index < querySize && query[index] != 0) {
        int labelLen = query[index++];
        for (int i = 0; i < labelLen && index < querySize; ++i) {
            queryDomain[j++] = query[index++];
        }
        if (query[index] != 0) {
            queryDomain[j++] = '.';
        }
    }
    queryDomain[j] = '\0';
    return j > 0;
}

bool findDomainIP(const char* domain, IPAddress& outIpAddress) {
    for (int i = 0; i < DOMAIN_MAPPINGS_COUNT; i++) {
        if (strcasecmp(domain, domainMappings[i].domainName) == 0) {
            outIpAddress = domainMappings[i].ipAddress;
            return true;
        }
    }
    return false;
}

void addCacheEntry(const char* domain, const IPAddress& ip) {
    for (int i = DNS_CACHE_SIZE - 1; i > 0; i--) {
        dnsCache[i] = dnsCache[i - 1];
    }
    strncpy(dnsCache[0].domainName, domain, sizeof(dnsCache[0].domainName) - 1);
    dnsCache[0].ipAddress = ip;
}

void logToSDCard(const char* logMessage) {
    File logFile;
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        String fileName = String(LOG_FILE_BASE_NAME) + i + LOG_FILE_EXTENSION;
        logFile = SD.open(fileName, FILE_WRITE);
        if (logFile && logFile.size() < MAX_LOG_FILE_SIZE) {
            break;
        }
        logFile.close();
    }

    if (logFile) {
        logFile.println(logMessage);
        logFile.close();
    } else {
        Serial.println("Failed to open log file for writing.");
        rotateLogs();
    }
}

void rotateLogs() {
    for (int i = MAX_LOG_FILES - 1; i > 0; i--) {
        String oldName = String(LOG_FILE_BASE_NAME) + i + LOG_FILE_EXTENSION;
        if (SD.exists(oldName)) {
            SD.remove(oldName);
        }
        String newName = String(LOG_FILE_BASE_NAME) + (i - 1) + LOG_FILE_EXTENSION;
        if (SD.exists(newName)) {
            SD.rename(newName, oldName);
        }
    }
}

bool findInLogs(const char* domain, IPAddress& ipAddress) {
    File logFile;
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        String fileName = String(LOG_FILE_BASE_NAME) + i + LOG_FILE_EXTENSION;
        logFile = SD.open(fileName, FILE_READ);
        if (!logFile) {
            continue;
        }

        while (logFile.available()) {
            String logEntry = logFile.readStringUntil('\n');
            int domainPos = logEntry.indexOf("Query: ");
            int ipPos = logEntry.indexOf(" IP: ");
            if (domainPos != -1 && ipPos != -1) {
                String foundDomain = logEntry.substring(domainPos + 7, ipPos);
                if (foundDomain.equalsIgnoreCase(domain)) {
                    String ipString = logEntry.substring(ipPos + 5);
                    ipAddress.fromString(ipString);
                    logFile.close();
                    return true;
                }
            }
        }
        logFile.close();
    }
    return false;
}

void sendDnsResponse(WiFiUDP& udp, const byte* response, int responseSize) {
    if (responseSize > 0) {
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.write(response, responseSize);
        udp.endPacket();
    }
}
