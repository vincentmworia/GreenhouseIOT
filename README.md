Here is the corrected and fully consistent README.md.
The problem was that some code blocks were not properly closed or started, causing everything after section 2 to collapse into plain text or wrong formatting in your editor/viewer.
Markdown# 🛰️ Secure MQTT Broker + Python Client – Mosquitto + TLS on Google Cloud

**Production-grade yet free-tier-friendly MQTT infrastructure** for IoT, home automation, school projects, prototyping, and small-scale deployments.

[![Python](https://img.shields.io/badge/python-3.8+-blue?logo=python&logoColor=white)](https://www.python.org)
[![Paho MQTT](https://img.shields.io/badge/paho--mqtt-2.0+-green)](https://pypi.org/project/paho-mqtt/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## ✨ Features

- Mosquitto broker on Google Cloud **e2-micro** (always-free eligible)
- **TLS encryption** on port 8883 with proper **SAN** support (no hostname errors)
- Username + password authentication (per-device recommended)
- **Last Will and Testament** (LWT) + retained online/offline presence
- Robust reconnecting **Python client** (`paho-mqtt`) with:
  - Exponential backoff
  - Offline message queuing (QoS 1)
  - Auto re-subscription
  - Birth / death messages
- Simple greenhouse sensor + command demo
- Full security & operations checklist

Goal: realistic secure patterns — simple, documented, **free-tier compatible**.

---

## 📁 Repository Structure
GreenhouseProject/
├── README.md
├── LICENSE                 # MIT  
├── .gitignore
├── .env.example            # copy to .env and fill secrets
├── certs/                  # .gitignore'd – CA & server certs go here (never commit!)
│
├── src/
│   ├── init.py
│   └── secure_mqtt.py      # production-ready MQTT client wrapper
│
└── code.py                 # example/demo application
text| File                        | Purpose                                                                 |
|-----------------------------|-------------------------------------------------------------------------|
| `src/secure_mqtt.py`        | TLS, LWT, reconnect, presence, safe publish wrapper                     |
| `code.py`                   | Demo: telemetry publishing + command handling                           |
| `.env`                      | Broker host, port, credentials, CA path                                 |
| `certs/ca.crt`              | CA certificate (client uses to verify broker)                           |

---

## 🚀 Quick Start (once VM is ready)

1. Set up Google Cloud **e2-micro** VM (only us-central1/us-east1/us-west1 regions are free)
2. Install & configure Mosquitto + TLS (see below)
3. Generate/copy certificates with correct **SAN**
4. Copy `.env.example` → `.env` and fill values
5. `pip install paho-mqtt python-dotenv`
6. Run `python code.py`

Expected output:
[2026-01-28 20:45] Connected! Publishing birth message...
[2026-01-28 20:45] Presence → greenhouse/devices/raspi-1-ecole-iot : online (retained)
[2026-01-28 20:55] Published telemetry → greenhouse/telemetry/counter : 42
...
text---

## ☁️ 1. Google Cloud VM – Free Tier Rules (2026)

| Setting             | Required                             | Reason / Risk                                      |
|---------------------|--------------------------------------|----------------------------------------------------|
| Machine type        | `e2-micro`                           | Only always-free shared-core type                  |
| Region              | us-central1, us-east1, us-west1      | Only these offer free e2-micro (1 per billing account) |
| Boot disk           | Standard persistent disk             | —                                                  |
| Disk size           | ≤ 30 GB                              | Exceeding → charges apply                          |
| Number of VMs       | 1                                    | Additional VMs → billing                           |

**Tip:** Set up billing alerts and budgets — even small deviations can trigger charges.

### Firewall (mandatory)

Ingress rule:
- Protocol/Port: TCP 8883
- Source: `0.0.0.0/0` (or restrict to your IP range for better security)

**Never expose port 1883 to the internet.**

---

## 🔐 2. Mosquitto Setup & Secure Config

# Update & install Mosquitto
sudo apt update && sudo apt upgrade -y
sudo apt install -y mosquitto mosquitto-clients

# Enable & start service
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
Create users (one per device recommended)
Bashsudo mosquitto_passwd -c /etc/mosquitto/passwd greenhouse-broker   # admin user
sudo mosquitto_passwd /etc/mosquitto/passwd raspi-1
sudo mosquitto_passwd /etc/mosquitto/passwd sensor-02
# ...
Minimal secure configuration file
conf# /etc/mosquitto/mosquitto.conf

allow_anonymous     false
password_file       /etc/mosquitto/passwd

# Optional: local plaintext listener (admin/debug only)
listener            1883  127.0.0.1

# Secure public listener (TLS)
listener            8883
cafile              /etc/mosquitto/certs/ca.crt
certfile            /etc/mosquitto/certs/server.crt
keyfile             /etc/mosquitto/certs/server.key

require_certificate false
Bash# Apply changes
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
Certificates – Critical (most common failure point)
You need three files in /etc/mosquitto/certs/:

ca.crt       – CA certificate (clients trust this)
server.crt   – Server certificate with SAN matching public IP or domain
server.key   – Server private key

Modern clients reject connections without matching SAN → hostname verification failed.
Quick self-signed example (testing only)
Bash# Run on local machine or VM – replace <YOUR_PUBLIC_IP>
openssl req -x509 -newkey rsa:4096 -nodes -days 3650 \
  -keyout server.key -out server.crt \
  -subj "/CN=greenhouse-broker" \
  -addext "subjectAltName = IP:<YOUR_PUBLIC_IP>"
Then copy server.crt & server.key to the VM and make ca.crt a copy of server.crt.
Production: Use Let's Encrypt + a real domain (free & trusted).

##  🧪 3. Quick Broker Test (from laptop)
    '''mosquitto_sub -h <YOUR_VM_PUBLIC_IP> \
              -p 8883 \
              -t '#' \
              -v \
              -u raspi-1 \
              -P StrongPass123! \
              --cafile ./certs/ca.crt
Success = connection stays open (or you see messages).

## 🌱 4. Device Presence (Birth + Last Will)
Use retained messages:
EventPublisherTopic examplePayloadRetainedQoSBirthClientgreenhouse/devices/<client_id>onlineyes1Death (LWT)Brokergreenhouse/devices/<client_id>offlineyes1
LWT ensures correct status even after crash/power loss/network drop.

## 🧩 5. Python Client (src/secure_mqtt.py)
Key features:

TLS + proper CA verification
Username/password auth
Automatic reconnect + exponential backoff
Birth message + Last Will and Testament
Offline publish queue (QoS 1)
Clean callback-based routing

Configuration via simple dataclass.

## 🏁 6. Demo Application (code.py)

Secure connection + birth message
Retained online presence
Dummy telemetry every ~10 seconds
Subscribes to greenhouse/commands/<client_id>/#

Example topics:
textgreenhouse/devices/raspi-1-ecole-iot           # presence (retained)
greenhouse/telemetry/counter                    # sensor data
greenhouse/commands/raspi-1-ecole-iot/led       # command example

## 🛠️ 7. Troubleshooting & Hard Reset
Clear retained messages, sessions & queued QoS:
Bashsudo systemctl stop mosquitto
sudo rm -f /var/lib/mosquitto/mosquitto.db
sudo systemctl start mosquitto

## 🔒 8. Security & Operations Checklist

 Never commit .env, certs/, keys
 Unique strong password per device
 Restrict firewall to trusted IPs when possible
 Always use unique client_id
 Use QoS 1 for important messages
 Rotate credentials periodically
 Monitor VM CPU/memory (e2-micro is shared → throttling possible) 