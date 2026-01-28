import os
import time
from datetime import UTC, datetime

from src.secure_mqtt import MqttConfig, SecureMqttClient
from dotenv import load_dotenv
load_dotenv()


CLIENT_ID = "raspi-1-ecole-iot"

# Load environment variables from .env

BROKER_HOST = os.getenv("MQTT_BROKER_HOST")
BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", "8883"))
USERNAME = os.getenv("MQTT_USERNAME")
PASSWORD = os.getenv("MQTT_PASSWORD")
CA_CERT = os.getenv("MQTT_CA_CERT")

# Device Online/Offline
PRESENCE_TOPIC = f"greenhouse/devices/{CLIENT_ID}"
BIRTH_PAYLOAD = "online"
DEATH_PAYLOAD = "offline"


def route(topic: str, payload: str) -> None:
    # TODO: listen to remote instructions
    if topic.startswith("greenhouse/commands/"):
        print(f"Command: {payload}")
        # TODO: handle command
    elif topic.startswith("greenhouse/sensors/"):
        print(f"Sensor: {payload}")
    else:
        print(f"{topic}: {payload}")


def main() -> None:
    config = MqttConfig(
        host=BROKER_HOST,
        port=BROKER_PORT,
        username=USERNAME,
        password=PASSWORD,
        ca_cert=CA_CERT,
        client_id=CLIENT_ID,
        subscribe_topic="#",  # for real devices, it’s better to subscribe only to what you need
        subscribe_qos=1,
        birth_topic=PRESENCE_TOPIC,
        birth_payload=BIRTH_PAYLOAD,
        death_topic=PRESENCE_TOPIC,
        death_payload=DEATH_PAYLOAD,
        presence_qos=1,
        presence_retain=True,
    )

    client = SecureMqttClient(config, on_message=route)
    client.start()

    counter = 1
    try:
        while True:
            # TODO: in real project publish when an event occurs
            timestamp = datetime.now(UTC).isoformat()
            payload = f"{counter} @ {timestamp}"

            sent = client.publish_text(f"greenhouse/telemetry/counter", payload, qos=1, retain=False)
            if sent:
                counter += 1

            time.sleep(10)

    except KeyboardInterrupt:
        pass
    finally:
        client.stop()


if __name__ == "__main__":
    main()
