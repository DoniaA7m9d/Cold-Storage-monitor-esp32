# Fridge / Cold Storage Monitor (V1)

An ESP32-based monitoring system that alerts in real time when a refrigeration
unit storing perishable produce fails — before spoilage happens.

## The Problem

A refrigerator storing agricultural produce lost power overnight. Nobody
noticed until the morning, by which point the stock had spoiled — a loss
that could reach 10,000–20,000 EGP in a single night. This kind of failure
is common and largely preventable with low-cost, always-on monitoring.

## How It Works

- A DHT11 sensor continuously monitors temperature and humidity inside the
  unit.
- If the temperature exceeds a defined threshold (default: 8°C) for several
  consecutive readings, the system treats it as a confirmed fault (not
  sensor noise) and triggers an alert.
- A local buzzer sounds immediately, independent of network status.
- A Telegram message is sent the moment the alert starts, and again once
  conditions return to normal (edge-triggered, not repeated every cycle).
- Every reading is logged to Google Sheets for a historical record.

## Design Notes

- **No deep sleep.** Unlike battery-powered field sensors, this device sits
  on the same always-on circuit as the refrigerator, and the priority is
  fast detection over power saving — deep sleep cycles would delay alerts
  by minutes, defeating the purpose.
- **Confirmation before alarm.** The system requires multiple consecutive
  high readings before alerting, to avoid false alarms from single noisy
  sensor readings.

## Hardware

| Component | Notes |
|---|---|
| ESP32 | Main controller |
| DHT11 | Temperature & humidity sensor |
| Buzzer | Local, immediate alert |

## Wiring

| Component | ESP32 Pin |
|---|---|
| DHT11 VCC | 3.3V |
| DHT11 GND | GND |
| DHT11 DATA | GPIO 4 |
| Buzzer + | GPIO 5 |
| Buzzer – | GND |

## Software Stack

- Arduino (ESP32 core)
- [DHT sensor library](https://github.com/adafruit/DHT-sensor-library) (Adafruit)
- [UniversalTelegramBot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)
- Google Apps Script (Web App) for Sheets logging

## Setup

1. Flash `fridge_cold_storage_monitor.ino` to the ESP32, filling in your
   WiFi credentials, Telegram bot token/chat ID, and Google Apps Script URL.
2. Deploy the included Apps Script as a Web App (`Anyone` access) linked to
   a Google Sheet with columns: `Timestamp | Temp | Humidity | Status`.
3. Power the device from the same outlet as the refrigerator (or a nearby
   always-on socket) so it keeps reporting even if the fridge itself trips.

## Status

This is a V1 built with a basic DHT11 sensor for practice and demonstration
purposes. A fuller version would add:
- A vibration or current sensor on the compressor motor for earlier fault
  detection (before the internal temperature has time to rise)
- A door-open sensor
- Battery backup so the ESP32 keeps reporting through a full power outage,
  not just a motor failure
