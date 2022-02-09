#include <gtest/gtest.h>

#include <unordered_map>

extern "C" {
#include "../Bootstrapper.h"
#include "../ProjectDescription.h"
}

struct FakeUserdata {
    std::set<std::string> existing_files;
    std::unordered_map<std::string, std::string> hashes;
};

static int FakeStartGDBServer(void*) { return 1; }
static int FakeStopGDBServer(void*) { return 1; }
static int FakeFileExists(const char* file_name, void* userdata) {
    if (userdata) {
        const auto* fake_userdata = static_cast<FakeUserdata*>(userdata);
        return fake_userdata->existing_files.find(file_name) != fake_userdata->existing_files.end();
    }
    return 1;
}
static void FakeCalculateHash(const char* file_name, char** hash, size_t* hash_size, void* userdata) {
    if (userdata) {
        const auto* fake_userdata = static_cast<FakeUserdata*>(userdata);
        const auto hashIt = fake_userdata->hashes.find(file_name);
        ASSERT_NE(hashIt, fake_userdata->hashes.end()) << "Forgot to add a hash for file '" << file_name << "'?";
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

    struct DynamicStringArray created_files, created_actual_hashes, created_wanted_hashes;
    DynamicStringArrayInit(&created_files);
    DynamicStringArrayInit(&created_actual_hashes);
    DynamicStringArrayInit(&created_wanted_hashes);
    ReportDifferentFiles(&given_bootstrapper, &created_files, &created_actual_hashes, &created_wanted_hashes);
    DynamicStringArrayDeinit(&created_files);
    DynamicStringArrayDeinit(&created_actual_hashes);
    DynamicStringArrayDeinit(&created_wanted_hashes);

    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, ReceiveNewProjectDescription) {
    struct Bootstrapper given_bootstrapper = {
        NULL, &FakeStartGDBServer, &FakeStopGDBServer, &FakeFileExists, &FakeCalculateHash, NULL};
    BootstrapperInit(&given_bootstrapper);

    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "LightSpeedFileExplorer", "abcd");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);
    ASSERT_TRUE(GetProjectDescription(&given_bootstrapper));

    EXPECT_TRUE(IsProjectLoaded(&given_bootstrapper));
    EXPECT_FALSE(IsGDBServerUp(&given_bootstrapper));

    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, ReceiveNewProjectDescriptionTwice) {
    struct Bootstrapper given_bootstrapper = {
        NULL, &FakeStartGDBServer, &FakeStopGDBServer, &FakeFileExists, &FakeCalculateHash, NULL};
    BootstrapperInit(&given_bootstrapper);

    struct ProjectDescription given_first_description;
    ProjectDescriptionInit(&given_first_description, "LightSpeedFileExplorer", "abcd");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_first_description);

    struct ProjectDescription given_second_description;
    ProjectDescriptionInit(&given_second_description, "DebuggerBootstrap", "defgh");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_second_description);
    ASSERT_TRUE(GetProjectDescription(&given_bootstrapper));
    EXPECT_EQ(std::string("DebuggerBootstrap"), GetProjectDescription(&given_bootstrapper)->executable_name);

    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, ReportMissingFiles) {
    FakeUserdata given_userdata{{"LightSpeedFileExplorer", "freetype.so"},
                                {{"LightSpeedFileExplorer", "abcd"}, {"freetype.so", "efgh"}}};

    struct Bootstrapper given_bootstrapper = {static_cast<void*>(&given_userdata),
                                              &FakeStartGDBServer,
                                              &FakeStopGDBServer,
                                              &FakeFileExists,
                                              &FakeCalculateHash,
                                              NULL};
    BootstrapperInit(&given_bootstrapper);
    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "LightSpeedFileExplorer", "abcd");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "freetype.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "zlib.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "libpng.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "efgh");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "ijkl");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "mnop");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);

    struct DynamicStringArray created_missing_files;
    ReportMissingFiles(&given_bootstrapper, &created_missing_files);

    ASSERT_EQ(2u, created_missing_files.size);
    EXPECT_EQ(std::string("zlib.so"), created_missing_files.data[0]);
    EXPECT_EQ(std::string("libpng.so"), created_missing_files.data[1]);

    given_userdata.existing_files.erase(given_userdata.existing_files.find("freetype.so"));

    // It's not this function's responsibility to check again whether there are new missing files
    ReportMissingFiles(&given_bootstrapper, &created_missing_files);
    ASSERT_EQ(2u, created_missing_files.size);
    EXPECT_EQ(std::string("zlib.so"), created_missing_files.data[0]);
    EXPECT_EQ(std::string("libpng.so"), created_missing_files.data[1]);

    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, ReportDifferentFiles_NoDifferences) {
    FakeUserdata given_userdata{
        {"LightSpeedFileExplorer", "freetype.so", "zlib.so", "libpng.so"},
        {{"LightSpeedFileExplorer", "abcd"}, {"freetype.so", "efgh"}, {"zlib.so", "ijkl"}, {"libpng.so", "mnop"}}};

    struct Bootstrapper given_bootstrapper = {static_cast<void*>(&given_userdata),
                                              &FakeStartGDBServer,
                                              &FakeStopGDBServer,
                                              &FakeFileExists,
                                              &FakeCalculateHash,
                                              NULL};

    BootstrapperInit(&given_bootstrapper);
    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "LightSpeedFileExplorer", "abcd");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "freetype.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "zlib.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable, "libpng.so");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "efgh");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "ijkl");
    DynamicStringArrayAppend(&given_description.link_dependencies_for_executable_hashes, "mnop");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);

    struct DynamicStringArray created_files, created_actual_hashes, created_wanted_hashes;
    DynamicStringArrayInit(&created_files);
    DynamicStringArrayInit(&created_actual_hashes);
    DynamicStringArrayInit(&created_wanted_hashes);
    ReportDifferentFiles(&given_bootstrapper, &created_files, &created_actual_hashes, &created_wanted_hashes);

    EXPECT_EQ(0, created_files.size);
    EXPECT_EQ(0, created_actual_hashes.size);
    EXPECT_EQ(0, created_wanted_hashes.size);

    DynamicStringArrayDeinit(&created_files);
    DynamicStringArrayDeinit(&created_actual_hashes);
    DynamicStringArrayDeinit(&created_wanted_hashes);
    BootstrapperDeinit(&given_bootstrapper);
}