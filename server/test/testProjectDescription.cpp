#include <gtest/gtest.h>

extern "C" {
#include "../ProjectDescription.h"
#include "../ProjectDescription_json.h"
}

namespace {
struct ProjectDescriptionRAII {
    ~ProjectDescriptionRAII() { ProjectDescriptionDeinit(&description); }

    ProjectDescription description;
};
} // namespace

TEST(testProjectDescription, LoadFromJSON) {
    ProjectDescriptionRAII created_description;

    const char* json_string = "{\"executable_name\": \"LightSpeedFileExplorer\", \"link_dependencies_for_executable\": "
                              "[\"freetype.so\", \"libpng.so\"], \"link_dependencies_for_executable_hashes\": "
                              "[\"abcd\", \"efgh\"]}";
    ASSERT_TRUE(ProjectDescriptionLoadFromJSON(json_string, &created_description.description));

    EXPECT_EQ(std::string(created_description.description.executable_name), "LightSpeedFileExplorer");

    ASSERT_EQ(2u, created_description.description.link_dependencies_for_executable.size);
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable.data[0]), "freetype.so");
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable.data[1]), "libpng.so");

    ASSERT_EQ(2u, created_description.description.link_dependencies_for_executable_hashes.size);
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable_hashes.data[0]), "abcd");
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable_hashes.data[1]), "efgh");
}

TEST(testProjectDescription, LoadFromIncompleteJSON) {
    ProjectDescription created_description;

    ASSERT_FALSE(ProjectDescriptionLoadFromJSON("{\"link_dependencies_for_executable\": "
                                                "[\"freetype.so\", \"libpng.so\"]}",
                                                &created_description));

    ASSERT_FALSE(
        ProjectDescriptionLoadFromJSON("{\"executable_name\": \"LightSpeedFileExplorer\"", &created_description));
}

TEST(testProjectDescription, LoadFromJSONWithWrongTypes) {
    ProjectDescription created_description;

    ASSERT_FALSE(ProjectDescriptionLoadFromJSON(
        "{\"executable_name\": \"LightSpeedFileExplorer\", \"link_dependencies_for_executable\": "
        "{}}",
        &created_description));

    ASSERT_FALSE(ProjectDescriptionLoadFromJSON("{\"executable_name\": [], \"link_dependencies_for_executable\": "
                                                "[\"freetype.so\", \"libpng.so\"]}",
                                                &created_description));
}

TEST(testProjectDescription, LoadFromJSON_NoLinkTimeDeps) {
    ProjectDescriptionRAII created_description;

    ASSERT_TRUE(ProjectDescriptionLoadFromJSON(
        "{\"executable_name\": \"LightSpeedFileExplorer\", \"link_dependencies_for_executable\": "
        "[], \"link_dependencies_for_executable_hashes\": "
        "[]}",
        &created_description.description));

    EXPECT_EQ(0, created_description.description.link_dependencies_for_executable.size);
}

TEST(testProjectDescription, DumpToJSON) {
    ProjectDescriptionRAII given_description;

    ProjectDescriptionInit(&given_description.description, "DebuggerBootstrap");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable, "first.so");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable, "second.so");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable, "third.so");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable_hashes, "abcd");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable_hashes, "efgh");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable_hashes, "ijkl");

    char* created_json_dump = ProjectDescriptionDumpToJSON(&given_description.description);
    EXPECT_EQ(std::string("{ \"executable_name\": \"DebuggerBootstrap\", \"link_dependencies_for_executable\": "
                          "[ \"first.so\", \"second.so\", \"third.so\" ], \"link_dependencies_for_executable_hashes\": "
                          "[ \"abcd\", \"efgh\", \"ijkl\" ] }"),
              created_json_dump);
    free(created_json_dump);
}