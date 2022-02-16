#include "FileHasher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/sha.h>

static void PutSHA1BytesIntoAllocatedString(const unsigned char* hash_bytes, char** hash_string, size_t* hash_length) {
    *hash_length = SHA_DIGEST_LENGTH * 2;
    *hash_string = (char*)malloc(sizeof(char) * *hash_length + 1);

    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        sprintf(&(*hash_string)[i * 2], "%x", hash_bytes[i]);
    }
}

void FileHasher_Do(const char* file, char** hash, size_t* hash_length) {
    FILE* file_handle = fopen(file, "rb");
    if (!file_handle) {
        *hash = "";
        *hash_length = 0;
        return;
    }
    unsigned char read_buffer[512];
    SHA_CTX sha1_context;
    SHA1_Init(&sha1_context);
    size_t bytes_read;
    while ((bytes_read = fread(read_buffer, sizeof(char), 512, file_handle)) > 0) {
        SHA1_Update(&sha1_context, read_buffer, bytes_read);
    }

    unsigned char hash_buffer[SHA_DIGEST_LENGTH];
    SHA1_Final(hash_buffer, &sha1_context);

    PutSHA1BytesIntoAllocatedString(&hash_buffer[0], hash, hash_length);

    fclose(file_handle);
}