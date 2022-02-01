#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

void CalculateSHA1Hash(const char* data, size_t dataLength, unsigned char* hash)
{
	SHA1(data, dataLength, hash);	
}

int main(int argc, char** argv)
{
	const char* message = "hello, omg it's c time\n";
	unsigned char hash[SHA_DIGEST_LENGTH];
	CalculateSHA1Hash(message, strlen(message), hash);
	printf("%s\n", message);
	for(int i=0; i<SHA_DIGEST_LENGTH; ++i)
	{
		printf("%x", hash[i]);
	}
	printf("\n");
	return 0;
}
