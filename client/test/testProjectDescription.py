import os
import unittest
import testenv
import ProjectDescription

class FakeProjectDescriptionFileHasher(ProjectDescription.ProjectDescriptionFileHasher):
    def __init__(self, hash_mapping):
        self.hash_mapping = hash_mapping

    def calculate_hash_for_file(self, file_path):
        return self.hash_mapping[file_path]

class FakeProjectDescriptionFileWalker(ProjectDescription.ProjectDescriptionFileWalker):
    def __init__(self, flat_file_structure):
        self.flat_file_structure = flat_file_structure

    def get_files_recursively_from_dir(self, predicate):
        return [item for item in self.flat_file_structure if predicate(os.path.basename(item))]

    def file_exists(self, file):
        return file in self.flat_file_structure

class TestProjectDescription(unittest.TestCase):

    def test_file_extension_matches(self):
        self.assertTrue(ProjectDescription._file_extension_matches("lib.so", ProjectDescription.SHARED_LIBRARY_EXTENSIONS))
        self.assertTrue(ProjectDescription._file_extension_matches("lib.so.6", ProjectDescription.SHARED_LIBRARY_EXTENSIONS))
        self.assertFalse(ProjectDescription._file_extension_matches("banana", ProjectDescription.SHARED_LIBRARY_EXTENSIONS))

    def test_gather_recursively_from_current_dir(self):
        given_hasher = FakeProjectDescriptionFileHasher({os.path.join("release","runme"): "jkl", "lib.so": "abc", "banana": "def", os.path.join("deps", "dep.so.4"): "ghi"})
        given_file_walker = FakeProjectDescriptionFileWalker(["lib.so", "banana", os.path.join("deps","dep.so.4"), os.path.join("release", "runme")])

        created_description = ProjectDescription.gather_recursively_from_current_dir(os.path.join("release", "runme"), ProjectDescription.SHARED_LIBRARY_EXTENSIONS, given_hasher, given_file_walker)
        self.assertEqual({"executable_name": "release/runme", "executable_hash": "jkl", "link_dependencies_for_executable": ["lib.so", "deps/dep.so.4"], "link_dependencies_for_executable_hashes": ["abc", "ghi"]}, created_description)
