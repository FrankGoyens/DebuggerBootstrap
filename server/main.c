#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <ftw.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <json.h>

#include "DynamicStringArray.h"
#include "EventDispatch.h"
#include "ProjectDescription.h"
#include "ProjectDescription_json.h"

#include "FileHasher.h"

static _Thread_local struct DynamicStringArray ftw_result_store;

static int display_info(const char* fpath, const struct stat* sb, int tflag, struct FTW* ftwbuf) {
    if (tflag == FTW_F)
        DynamicStringArrayAppend(&ftw_result_store, fpath);
    else if (tflag == FTW_D && ftwbuf->level > 0)
        return FTW_SKIP_SUBTREE;
    return FTW_CONTINUE;
}

void CalculateSHA1Hash(const char* data, size_t dataLength, unsigned char* hash) { SHA1(data, dataLength, hash); }

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s PATH [PATH ...]\n", argv[0]);
        return 1;
    }

    DynamicStringArrayInit(&ftw_result_store);
    nftw(argv[1], display_info, 1, FTW_ACTIONRETVAL);
    for (int i = 0; i < ftw_result_store.size; ++i)
        printf("Entry: %s\n", ftw_result_store.data[i]);
    DynamicStringArrayDeinit(&ftw_result_store);

    const char* message = "hello, omg it's c time\n";

    unsigned char hash[SHA_DIGEST_LENGTH];
    CalculateSHA1Hash(message, strlen(message), hash);
    printf("%s\n", message);
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        printf("%x", hash[i]);
    }
    printf("\n");

    char* file_hash;
    size_t hash_length;
    FileHasher_Do(argv[0], &file_hash, &hash_length);
    printf("File hash: %s\n", file_hash);
    free(file_hash);

    // StartGDBServerProcess();
    // StartEventDispatch(1337);

    return 0;
}
