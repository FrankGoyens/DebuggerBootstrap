import hashlib
import os

def calculate_file_hash(file):
    if not os.path.exists(file):
        return ""

    read_amount = 512
    hash = hashlib.sha1()
    with open(file, 'rb') as file_handle:
        
        while True:
            data = file_handle.read(read_amount)
            if(len(data)<=0):
                break
            hash.update(data)
    return hash.hexdigest()