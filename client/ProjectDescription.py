import os
from abc import ABC, abstractmethod
import fnmatch
import FileHasher

SHARED_LIBRARY_EXTENSIONS = ["*.so*"]

class ProjectDescriptionFileHasher(ABC):
    @abstractmethod
    def calculate_hash_for_file(self, file_path):
        """Returns a string representing the hash for the file. None when there was an error."""
        pass

class ProjectDescriptionFileWalker(ABC):
    @abstractmethod
    def get_files_recursively_from_dir(self, predicate):
        """Returns a list of tuples (in traversal order) of all files that satisfy the given predicate
        
        predicate takes a file name as argument, and returns a bool. True means valid, False means invalid.
        """
        pass

    @abstractmethod
    def file_exists(self, file):
        pass

class DefaultProjectDescriptionFileHasher(ProjectDescriptionFileHasher):
    def calculate_hash_for_file(file_path):
        return FileHasher.calculate_file_hash(file_path)

class DefaultProjectDescriptionFileWalker(ProjectDescriptionFileWalker):
    def get_files_recursively_from_dir(predicate):
        files_matching_predicate = []
        for _, _, file in os.walk("."):
            if predicate(file):
                files_matching_predicate.append(file)
        return files_matching_predicate

    def file_exists(self, file):
        return os.path.isfile(file)

def _file_extension_matches(file_name, extensions):
    for extension in extensions:
        if fnmatch.fnmatch(file_name, extension):
            return True
    return False

def _find_link_dependencies_for_executable(shared_library_extensions, file_hasher, directory_walker):
    link_dependencies_for_executable_and_hashes = []
    for file in directory_walker.get_files_recursively_from_dir(lambda file: _file_extension_matches(file, shared_library_extensions)):
        hash = file_hasher.calculate_hash_for_file(file)
        if hash is not None:
            link_dependencies_for_executable_and_hashes.append((file, hash))
    return link_dependencies_for_executable_and_hashes

def gather_recursively_from_current_dir(executable_file, shared_library_extensions=SHARED_LIBRARY_EXTENSIONS, file_hasher=DefaultProjectDescriptionFileHasher(), directory_walker=DefaultProjectDescriptionFileWalker()):
    """ Make a project description using the given executable file and every other file with an extension from shared_library_extensions
    
    shared libraries are found by recursively walking through the current directory.
    The extensions are defined as wildcard expressions, for example '*.so*'.
    Use SHARED_LIBRARY_EXTENSIONS unless you use other extensions for your project.
    file_hasher is an implementation of ProjectDescriptionFileHasher. 
    directory_walker is an implementation of ProjectDescriptionFileWalker.
    """

    if not directory_walker.file_exists(executable_file):
        return None

    executable_file_norm = os.path.normpath(executable_file)
    executable_file_hash = file_hasher.calculate_hash_for_file(executable_file_norm)

    if executable_file_hash is None:
        return None

    project_description = {"executable_name": "/".join(executable_file_norm.split()), "executable_hash": executable_file_hash}

    link_dependencies_for_executable_and_hashes = _find_link_dependencies_for_executable(shared_library_extensions, file_hasher, directory_walker)

    project_description["link_dependencies_for_executable"] = [file_and_hash[0] for file_and_hash in link_dependencies_for_executable_and_hashes]
    project_description["link_dependencies_for_executable_hashes"] = [file_and_hash[1] for file_and_hash in link_dependencies_for_executable_and_hashes]

    return project_description