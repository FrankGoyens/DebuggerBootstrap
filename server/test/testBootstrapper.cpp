#include <gtest/gtest.h>

#include <unordered_map>

extern "C" {
#include "../Bootstrapper.h"
#include "../ProjectDescription.h"
}

static int FakeStartGDBServer(void*) { return 1; }
static int FakeStopGDBServer(void*) { return 1; }
static int FakeFileExists(const char* file_name, void* userdata) {
    if (userdata) {
        const auto* existing_files = static_cast<std::set<std::string>*>(userdata);
        return existing_files->find(file_name) != existing_files->end();
    }
    return 1;
}
static void FakeCalculateHash(const char* file_name, char** hash, size_t* hash_size, void* userdata) {
    if (userdata) {
        const auto* hashes = static_cast<std::unordered_map<std::string, std::string>*>(userdata);
        const auto hashIt = hashes->find(file_name);
        ASSERT_NE(hashIt, hashes->end()) << "Forgot to add a hash for file '" << file_name << "'?";
        *hash = (char*)malloc(hashIt->second.size());
        memcpy(*hash, hashIt->second.c_str(), hashIt->second.size());
        *hash_size = hashIt->second.size();
    }
}

TEST(testBootstrapper, Init) {
    struct Bootstrapper given_bootstrapper = {
        NULL, &FakeStartGDBServer, &FakeStopGDBServer, &FakeFileExists, &FakeCalculateHash, NULL};
    BootstrapperInit(&given_bootstrapper);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, InitialReporting) {
    struct Bootstrapper given_bootstrapper = {
        NULL, &FakeStartGDBServer, &FakeStopGDBServer, &FakeFileExists, &FakeCalculateHash, NULL};
    BootstrapperInit(&given_bootstrapper);
    EXPECT_FALSE(IsProjectLoaded(&given_bootstrapper));
    EXPECT_FALSE(IsGDBServerUp(&given_bootstrapper));
    ReportMissingFiles(&given_bootstrapper, NULL);
    ReportDifferentFiles(&given_bootstrapper, NULL, NULL, NULL);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, RecieveNewProjectDescription) {
    struct Bootstrapper given_bootstrapper = {
        NULL, &FakeStartGDBServer, &FakeStopGDBServer, &FakeFileExists, &FakeCalculateHash, NULL};
    BootstrapperInit(&given_bootstrapper);

    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "LightSpeedFileExplorer", "abcd");

    RecieveNewProjectDescription(&given_bootstrapper, &given_description);
    ASSERT_TRUE(GetProjectDescription(&given_bootstrapper));

    EXPECT_TRUE(IsProjectLoaded(&given_bootstrapper));
    EXPECT_FALSE(IsGDBServerUp(&given_bootstrapper));

    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, RecieveNewProjectDescriptionTwice) {
    struct Bootstrapper given_bootstrapper = {
        NULL, &FakeStartGDBServer, &FakeStopGDBServer, &FakeFileExists, &FakeCalculateHash, NULL};
    BootstrapperInit(&given_bootstrapper);

    struct ProjectDescription given_first_description;
    ProjectDescriptionInit(&given_first_description, "LightSpeedFileExplorer", "abcd");

    RecieveNewProjectDescription(&given_bootstrapper, &given_first_description);

    struct ProjectDescription given_second_description;
    ProjectDescriptionInit(&given_second_description, "DebuggerBootstrap", "defgh");

    RecieveNewProjectDescription(&given_bootstrapper, &given_second_description);
    ASSERT_TRUE(GetProjectDescription(&given_bootstrapper));
    EXPECT_EQ(std::string("DebuggerBootstrap"), GetProjectDescription(&given_bootstrapper)->executable_name);

    BootstrapperDeinit(&given_bootstrapper);
}