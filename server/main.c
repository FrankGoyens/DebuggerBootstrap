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

static _Thread_local struct DynamicStringArray ftw_result_store;

static int display_info(const char* fpath, const struct stat* sb, int tflag, struct FTW* ftwbuf) {
    if (tflag == FTW_F)
        DynamicStringArrayAppend(&ftw_result_store, fpath);
    else if (tflag == FTW_D && ftwbuf->level > 0)
        return FTW_SKIP_SUBTREE;
    return FTW_CONTINUE;
}

void CalculateSHA1Hash(const char* data, size_t dataLength, unsigned char* hash) { SHA1(data, dataLength, hash); }

void StartGDBServerProcess() {
    int pipefd[2];
    int pipefd_err[2];
    int p1 = pipe(pipefd);
    int p2 = pipe(pipefd_err);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd_err[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        close(pipefd_err[0]);
        close(pipefd_err[1]);
        char* args[] = {"gdbserver", "--help", NULL};
        char* env[] = {NULL};
        execve("/usr/bin/gdbserver", args, env);
    } else {
        close(pipefd[1]);
        close(pipefd_err[1]);
        printf("parent is sleeping...\n");

        char buffer[512];

        int bytes = 0;
        int bytes_err = 0;
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0)
            printf("Output: (%.*s)\n", bytes, buffer);
        while ((bytes_err = read(pipefd_err[0], buffer, sizeof(buffer))) > 0)
            printf("Output (err): (%.*s)\n", bytes, buffer);
    }
}

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

    StartGDBServerProcess();
    // StartEventDispatch(1337);

    return 0;
}
