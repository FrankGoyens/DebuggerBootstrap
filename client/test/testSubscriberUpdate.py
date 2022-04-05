import unittest
import testenv
import SubscriberUpdate

class TestSubscriberUpdate(unittest.TestCase):
    def test_invalid_json(self):
        self.assertRaises(SubscriberUpdate.SubscriberUpdateException, SubscriberUpdate.from_json, "bla")

    def test_tag_and_message(self):
        created_subscriber_update = SubscriberUpdate.from_json(r'{"tag": "testtag", "message": "testmessage"}')
        self.assertEquals("testtag", created_subscriber_update.tag)
        self.assertEquals("testmessage", created_subscriber_update.message)

    def test_tag_only(self):
        created_subscriber_update = SubscriberUpdate.from_json(r'{"tag": "testtag"}')
        self.assertEquals("testtag", created_subscriber_update.tag)
        self.assertEquals("NO MESSAGE", created_subscriber_update.message)

    def test_message_only(self):
        created_subscriber_update = SubscriberUpdate.from_json(r'{"message": "testmessage"}')
        self.assertEquals("UNTAGGED", created_subscriber_update.tag)
        self.assertEquals("testmessage", created_subscriber_update.message)

    def test_no_tag_or_message(self):
        created_subscriber_update = SubscriberUpdate.from_json(r'{}')
        self.assertEquals("UNTAGGED", created_subscriber_update.tag)
        self.assertEquals("NO MESSAGE", created_subscriber_update.message)

if __name__ == "__main__":
    unittest.main()