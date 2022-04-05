
import json

class SubscriberUpdateException(Exception):
    pass

class SubscriberUpdate(object):

    def __init__(self, **kwargs):
        self.tag = kwargs["tag"] if "tag" in kwargs else "UNTAGGED"
        self.message = kwargs["message"] if "message" in kwargs else "NO MESSAGE"
            

def from_json(json_string):
    try:
        subscriber_update_json = json.loads(json_string)
        return SubscriberUpdate(**subscriber_update_json)
    except json.JSONDecodeError as e:
        raise SubscriberUpdateException from e