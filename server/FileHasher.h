#pragma once

#include <stddef.h>

// Calculates the hash of the given file, puts the result as a c-string in 'hash', the length of the resulting string is
// put into 'hash_length'.
// When something goes wrong, like the file can't be openened, the hash_length is zero
// When hash_length is 0 the hash is NOT to be freed!
void FileHasher_Do(const char* file, char** hash, size_t* hash_length);