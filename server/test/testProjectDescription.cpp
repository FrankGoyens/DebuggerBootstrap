#include <optional>

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

TEST(testProjectDescription, EmptyName) {
    ProjectDescription given_description;
    ProjectDescriptionInit(&given_description, "", "");
    ProjectDescriptionDeinit(&given_description);
}

TEST(testProjectDescription, LoadFromJSON) {
    ProjectDescriptionRAII created_description;

    const char* json_string = "{\"executable_name\": \"LightSpeedFileExplorer\",\"executable_hash\": \"hijk\", "
                              "\"link_dependencies_for_executable\": "
                              "[\"freetype.so\", \"libpng.so\"], \"link_dependencies_for_executable_hashes\": "
                              "[\"abcd\", \"efgh\"]}";
    ASSERT_TRUE(ProjectDescriptionLoadFromJSON(json_string, &created_description.description));

    EXPECT_EQ(std::string(created_description.description.executable_name), "LightSpeedFileExplorer");
    EXPECT_EQ(std::string(created_description.description.executable_hash), "hijk");

    ASSERT_EQ(2u, created_description.description.link_dependencies_for_executable.size);
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable.data[0]), "freetype.so");
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable.data[1]), "libpng.so");

    ASSERT_EQ(2u, created_description.description.link_dependencies_for_executable_hashes.size);
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable_hashes.data[0]), "abcd");
    EXPECT_EQ(std::string(created_description.description.link_dependencies_for_executable_hashes.data[1]), "efgh");
}

TEST(testProjectDescription, LoadFromJSON_WithExecutableArguments) {
    ProjectDescriptionRAII created_description;

    const char* json_string = "{\"executable_name\": \"LightSpeedFileExplorer\",\"executable_hash\": \"hijk\", "
                              "\"link_dependencies_for_executable\": "
                              "[\"freetype.so\", \"libpng.so\"], \"link_dependencies_for_executable_hashes\": "
                              "[\"abcd\", \"efgh\"], \"executable_arguments\": [\"-a\", \"-b\", 5]}";

    ASSERT_TRUE(ProjectDescriptionLoadFromJSON(json_string, &created_description.description));

    ASSERT_EQ(2u, created_description.description.executable_arguments.size);
    EXPECT_EQ(std::string(created_description.description.executable_arguments.data[0]), "-a");
    EXPECT_EQ(std::string(created_description.description.executable_arguments.data[1]), "-b");
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

    ASSERT_TRUE(ProjectDescriptionLoadFromJSON("{\"executable_name\": \"LightSpeedFileExplorer\",\"executable_hash\": "
                                               "\"abcd\", \"link_dependencies_for_executable\": "
                                               "[], \"link_dependencies_for_executable_hashes\": "
                                               "[]}",
                                               &created_description.description));

    EXPECT_EQ(0, created_description.description.link_dependencies_for_executable.size);
}

static std::optional<std::tuple<size_t, char, char>> FindFirstDifferentElement(const std::string_view& first,
                                                                               const std::string_view& second) {
    const auto smallest_size = std::min(first.size(), second.size());
    for (size_t i = 0; i < smallest_size; ++i) {
        if (first[i] != second[i])
            return std::make_tuple(i, first[i], second[i]);
    }
    return {};
}

TEST(testProjectDescription, DumpToJSON) {
    ProjectDescriptionRAII given_description;

    ProjectDescriptionInit(&given_description.description, "DebuggerBootstrap", "mnop");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable, "first.so");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable, "second.so");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable, "third.so");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable_hashes, "abcd");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable_hashes, "efgh");
    DynamicStringArrayAppend(&given_description.description.link_dependencies_for_executable_hashes, "ijkl");
    DynamicStringArrayAppend(&given_description.description.executable_arguments, "-a");
    DynamicStringArrayAppend(&given_description.description.executable_arguments, "-b");
    DynamicStringArrayAppend(&given_description.description.executable_arguments, "-c");

    char* created_json_dump = ProjectDescriptionDumpToJSON(&given_description.description);
    std::string expected_json_dump(
        "{ \"executable_name\": \"DebuggerBootstrap\", \"executable_hash\": \"mnop\", "
        "\"link_dependencies_for_executable\": "
        "[ \"first.so\", \"second.so\", \"third.so\" ], \"link_dependencies_for_executable_hashes\": "
        "[ \"abcd\", \"efgh\", \"ijkl\" ], \"executable_arguments\": [ \"-a\", \"-b\", \"-c\" ] }");
    EXPECT_EQ(expected_json_dump, created_json_dump)
        << "Different character at: " << std::get<0>(*FindFirstDifferentElement(expected_json_dump, created_json_dump));
    free(created_json_dump);
}