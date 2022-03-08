from abc import ABC, abstractmethod

class MessageDecoder(ABC):
    @abstractmethod
    def receive_subscription_response(self, packet_length, message_json):
        pass

    @abstractmethod
    def receive_incomplete_response(self, incomplete_data_bytes):
        pass

    @abstractmethod
    def receive_unknown_response(self, unknown_data_bytes):
        pass