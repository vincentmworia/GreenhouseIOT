import ssl
from dataclasses import dataclass
from typing import Callable, Optional

import paho.mqtt.client as mqtt

MessageHandler = Callable[[str, str], None]


@dataclass(frozen=True)
class MqttConfig:
    host: str
    port: int
    username: str
    password: str
    ca_cert: str
    client_id: str

    keepalive: int = 60
    subscribe_topic: str = "#"
    subscribe_qos: int = 1

    # Presence (birth/death)
    birth_topic: Optional[str] = None
    birth_payload: Optional[str] = None
    death_topic: Optional[str] = None
    death_payload: Optional[str] = None
    presence_qos: int = 1
    presence_retain: bool = True

    # Reconnect backoff
    reconnect_min_delay: int = 1
    reconnect_max_delay: int = 30


class SecureMqttClient:
    def __init__(self, config: MqttConfig, on_message: Optional[MessageHandler] = None) -> None:
        self.config = config
        self._user_handler = on_message

        self._stopping = False
        self._connected = False

        self._client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=config.client_id,
            protocol=mqtt.MQTTv311,
        )

        # Last Will and Testament (published by the broker if the client dies unexpectedly)
        if config.death_topic is not None and config.death_payload is not None:
            self._client.will_set(
                topic=config.death_topic,
                payload=config.death_payload,
                qos=config.presence_qos,
                retain=config.presence_retain,
            )

        self._client.username_pw_set(config.username, config.password)
        self._client.tls_set(
            ca_certs=config.ca_cert,
            cert_reqs=ssl.CERT_REQUIRED,
            tls_version=ssl.PROTOCOL_TLS_CLIENT,
        )

        self._client.reconnect_delay_set(
            min_delay=config.reconnect_min_delay,
            max_delay=config.reconnect_max_delay,
        )

        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message
        self._client.on_disconnect = self._on_disconnect

    def start(self) -> None:
        self._stopping = False
        self._client.connect_async(self.config.host, self.config.port, keepalive=self.config.keepalive)
        self._client.loop_start()

    def stop(self) -> None:
        self._stopping = True

        if self._connected and self.config.death_topic and self.config.death_payload is not None:
            self._client.publish(
                self.config.death_topic,
                payload=self.config.death_payload,
                qos=self.config.presence_qos,
                retain=self.config.presence_retain,
            )

        self._client.disconnect()
        self._client.loop_stop()
        self._connected = False

    def is_connected(self) -> bool:
        return self._connected

    def publish_text(self, topic: str, payload: str, qos: int = 0, retain: bool = False, wait: bool = False) -> bool:
        if not self._connected:
            return False

        try:
            info = self._client.publish(topic, payload=payload, qos=qos, retain=retain)
        except (OSError, RuntimeError):
            self._connected = False
            return False

        if wait:
            try:
                info.wait_for_publish()
            except RuntimeError:
                self._connected = False
                return False

        return True

    def _on_connect(self, client: mqtt.Client, _userdata, _flags, reason_code, _properties) -> None:
        rc = getattr(reason_code, "value", reason_code)
        if rc != 0:
            self._connected = False
            print(f"Connect failed: {reason_code!r}")
            return

        self._connected = True
        print("Connected.")

        client.subscribe(self.config.subscribe_topic, qos=self.config.subscribe_qos)

        if self.config.birth_topic and self.config.birth_payload is not None:
            client.publish(
                self.config.birth_topic,
                payload=self.config.birth_payload,
                qos=self.config.presence_qos,
                retain=self.config.presence_retain,
            )

    def _on_message(self, _client: mqtt.Client, _userdata, message: mqtt.MQTTMessage) -> None:
        topic = message.topic
        raw = message.payload
        payload_bytes = raw if isinstance(raw, (bytes, bytearray, memoryview)) else str(raw).encode("utf-8")
        payload_text = payload_bytes.decode("utf-8", errors="replace")

        if self._user_handler is not None:
            self._user_handler(topic, payload_text)

    def _on_disconnect(self, _client, _userdata, _flags, reason_code, _properties) -> None:
        self._connected = False

        rc = getattr(reason_code, "value", reason_code)
        clean = self._stopping or rc == 0

        if clean:
            print(f"Disconnected: {reason_code!r}")
        else:
            print(f"Unexpected disconnect: {reason_code!r} (auto-reconnect enabled)")
