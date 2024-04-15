#ifndef LOCALDNS_H
#define LOCALDNS_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <SD.h>
#include <FreeRTOS.h>

bool startDnsServer();
void stopDnsServer();

#endif // LOCALDNS_H
