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
        *hash_size = hashIt->second.size();
        *hash = (char*)malloc(hashIt->second.size() + 1);
        strcpy(*hash, hashIt->second.c_str());
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
    ReportWantedVsActualHashes(&given_bootstrapper, &created_files, &created_actual_hashes, &created_wanted_hashes);
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

    ProjectDescriptionDeinit(&given_description);
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

    ProjectDescriptionDeinit(&given_first_description);
    ProjectDescriptionDeinit(&given_second_description);
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
    DynamicStringArrayInit(&created_missing_files);
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

    ProjectDescriptionDeinit(&given_description);
    DynamicStringArrayDeinit(&created_missing_files);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, ReportWantedVsActualHashes) {
    FakeUserdata given_userdata{
        {"LightSpeedFileExplorer", "freetype.so", "zlib.so", "libpng.so"},
        {{"LightSpeedFileExplorer", "mnop"}, {"freetype.so", "ijkl"}, {"zlib.so", "efgh"}, {"libpng.so", "abcd"}}};

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
    ReportWantedVsActualHashes(&given_bootstrapper, &created_files, &created_actual_hashes, &created_wanted_hashes);

    ASSERT_EQ(4, created_files.size);
    ASSERT_EQ(4, created_actual_hashes.size);
    ASSERT_EQ(4, created_wanted_hashes.size);

    EXPECT_EQ(std::string("LightSpeedFileExplorer"), created_files.data[0]);
    EXPECT_EQ(std::string("freetype.so"), created_files.data[1]);
    EXPECT_EQ(std::string("zlib.so"), created_files.data[2]);
    EXPECT_EQ(std::string("libpng.so"), created_files.data[3]);

    EXPECT_EQ(std::string("mnop"), created_actual_hashes.data[0]);
    EXPECT_EQ(std::string("ijkl"), created_actual_hashes.data[1]);
    EXPECT_EQ(std::string("efgh"), created_actual_hashes.data[2]);
    EXPECT_EQ(std::string("abcd"), created_actual_hashes.data[3]);

    EXPECT_EQ(std::string("abcd"), created_wanted_hashes.data[0]);
    EXPECT_EQ(std::string("efgh"), created_wanted_hashes.data[1]);
    EXPECT_EQ(std::string("ijkl"), created_wanted_hashes.data[2]);
    EXPECT_EQ(std::string("mnop"), created_wanted_hashes.data[3]);

    DynamicStringArrayDeinit(&created_files);
    DynamicStringArrayDeinit(&created_actual_hashes);
    DynamicStringArrayDeinit(&created_wanted_hashes);
    ProjectDescriptionDeinit(&given_description);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, ReportWantedVsActualHashes_OnlyOneExecutable) {
    FakeUserdata given_userdata;

    struct Bootstrapper given_bootstrapper = {static_cast<void*>(&given_userdata),
                                              &FakeStartGDBServer,
                                              &FakeStopGDBServer,
                                              &FakeFileExists,
                                              &FakeCalculateHash,
                                              NULL};

    BootstrapperInit(&given_bootstrapper);
    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "test_exe", "abc");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);

    struct DynamicStringArray created_files, created_actual_hashes, created_wanted_hashes;
    DynamicStringArrayInit(&created_files);
    DynamicStringArrayInit(&created_actual_hashes);
    DynamicStringArrayInit(&created_wanted_hashes);
    ReportWantedVsActualHashes(&given_bootstrapper, &created_files, &created_actual_hashes, &created_wanted_hashes);

    ASSERT_EQ(0, created_files.size);
    ASSERT_EQ(0, created_actual_hashes.size);
    ASSERT_EQ(0, created_wanted_hashes.size);

    given_userdata = {{"test_exe"}, {{"test_exe", "def"}}};

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);
    ReportWantedVsActualHashes(&given_bootstrapper, &created_files, &created_actual_hashes, &created_wanted_hashes);

    ASSERT_EQ(1, created_files.size);
    ASSERT_EQ(1, created_actual_hashes.size);
    ASSERT_EQ(1, created_wanted_hashes.size);

    EXPECT_EQ(std::string("test_exe"), created_files.data[0]);

    EXPECT_EQ(std::string("def"), created_actual_hashes.data[0]);

    EXPECT_EQ(std::string("abc"), created_wanted_hashes.data[0]);

    DynamicStringArrayDeinit(&created_files);
    DynamicStringArrayDeinit(&created_actual_hashes);
    DynamicStringArrayDeinit(&created_wanted_hashes);
    ProjectDescriptionDeinit(&given_description);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, UpdateFileActualHash) {
    FakeUserdata given_userdata{
        {"LightSpeedFileExplorer", "freetype.so", "zlib.so", "libpng.so"},
        {{"LightSpeedFileExplorer", "mnop"}, {"freetype.so", "ijkl"}, {"zlib.so", "efgh"}, {"libpng.so", "abcd"}}};

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

    ASSERT_FALSE(IsGDBServerUp(&given_bootstrapper));

    given_userdata.hashes["LightSpeedFileExplorer"] = "abcd";
    UpdateFileActualHash(&given_bootstrapper, "LightSpeedFileExplorer");
    given_userdata.hashes["freetype.so"] = "efgh";
    UpdateFileActualHash(&given_bootstrapper, "freetype.so");
    given_userdata.hashes["zlib.so"] = "ijkl";
    UpdateFileActualHash(&given_bootstrapper, "zlib.so");
    given_userdata.hashes["libpng.so"] = "mnop";
    UpdateFileActualHash(&given_bootstrapper, "libpng.so");

    EXPECT_TRUE(IsGDBServerUp(&given_bootstrapper));

    ProjectDescriptionDeinit(&given_description);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, UpdateFileActualHash_FileBecomesExistant) {
    FakeUserdata given_userdata{{}, {{"LightSpeedFileExplorer", "abcd"}}};

    struct Bootstrapper given_bootstrapper = {static_cast<void*>(&given_userdata),
                                              &FakeStartGDBServer,
                                              &FakeStopGDBServer,
                                              &FakeFileExists,
                                              &FakeCalculateHash,
                                              NULL};

    BootstrapperInit(&given_bootstrapper);
    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "LightSpeedFileExplorer", "abcd");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);

    ASSERT_FALSE(IsGDBServerUp(&given_bootstrapper));

    given_userdata.existing_files.insert("LightSpeedFileExplorer");
    UpdateFileActualHash(&given_bootstrapper, "LightSpeedFileExplorer");

    EXPECT_TRUE(IsGDBServerUp(&given_bootstrapper));

    ProjectDescriptionDeinit(&given_description);
    BootstrapperDeinit(&given_bootstrapper);
}

TEST(testBootstrapper, IndicateRemovedFile) {
    FakeUserdata given_userdata{{"LightSpeedFileExplorer"}, {{"LightSpeedFileExplorer", "abcd"}}};

    struct Bootstrapper given_bootstrapper = {static_cast<void*>(&given_userdata),
                                              &FakeStartGDBServer,
                                              &FakeStopGDBServer,
                                              &FakeFileExists,
                                              &FakeCalculateHash,
                                              NULL};

    BootstrapperInit(&given_bootstrapper);
    struct ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "LightSpeedFileExplorer", "abcd");

    ReceiveNewProjectDescription(&given_bootstrapper, &given_description);

    ASSERT_TRUE(IsGDBServerUp(&given_bootstrapper));

    IndicateRemovedFile(&given_bootstrapper, "LightSpeedFileExplorer");
    ASSERT_TRUE(IsGDBServerUp(&given_bootstrapper)); // The file actualy still exists

    given_userdata.existing_files.erase(given_userdata.existing_files.find("LightSpeedFileExplorer"));
    IndicateRemovedFile(&given_bootstrapper, "LightSpeedFileExplorer");
    ASSERT_FALSE(IsGDBServerUp(&given_bootstrapper));

    UpdateFileActualHash(&given_bootstrapper, "LightSpeedFileExplorer");
    IndicateRemovedFile(&given_bootstrapper, "LightSpeedFileExplorer");
    ASSERT_FALSE(IsGDBServerUp(&given_bootstrapper));

    given_userdata.existing_files.insert("LightSpeedFileExplorer");
    UpdateFileActualHash(&given_bootstrapper, "LightSpeedFileExplorer");
    ASSERT_TRUE(IsGDBServerUp(&given_bootstrapper));

    ProjectDescriptionDeinit(&given_description);
    BootstrapperDeinit(&given_bootstrapper);
}
