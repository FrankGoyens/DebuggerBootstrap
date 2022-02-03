#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <ftw.h>

#include <json.h>

#include "DynamicStringArray.h"
#include "ProjectDescription.h"

static _Thread_local struct DynamicStringArray ftw_result_store;

static int display_info(const char *fpath, const struct stat *sb,
             int tflag, struct FTW *ftwbuf)
{
	if(tflag==FTW_F)
		DynamicStringArrayAppend(&ftw_result_store, fpath);
	else if(tflag == FTW_D && ftwbuf->level>0)
    	return FTW_SKIP_SUBTREE;
	return FTW_CONTINUE;
}

void CalculateSHA1Hash(const char* data, size_t dataLength, unsigned char* hash)
{
	SHA1(data, dataLength, hash);	
}

int ProjectDescriptionFromJSON(struct ProjectDescription* project_description, const char* json_string){
	json_object* root = json_tokener_parse(json_string);

	json_object* executable_name_json = json_object_object_get(root, "executable_name");
	json_object* link_dependencies_for_executable_json = json_object_object_get(root, "link_dependencies_for_executable");

	if(executable_name_json==NULL || link_dependencies_for_executable_json==NULL)
		return 0;

	if(!json_object_is_type(executable_name_json, json_type_string))
		return 0;

	if(!json_object_is_type(link_dependencies_for_executable_json, json_type_array))
		return 0;

	ProjectDescriptionInit(project_description, json_object_get_string(executable_name_json));

	int array_length = json_object_array_length(link_dependencies_for_executable_json);
	for(int i=0; i<array_length;++i){
		json_object* dependency = json_object_array_get_idx(link_dependencies_for_executable_json, i);
		if(json_object_is_type(executable_name_json, json_type_string))
			DynamicStringArrayAppend(&project_description->link_dependencies_for_executable, json_object_get_string(dependency));
	}

	printf("The pretty formatted json:\n%s\n", json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY));
	json_object_put(root);

	return 1;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("Usage: %s PATH [PATH ...]\n", argv[0]);
		return 1;
	}

	DynamicStringArrayInit(&ftw_result_store);
	nftw(argv[1], display_info, 1, FTW_ACTIONRETVAL);
	for(int i=0; i<ftw_result_store.size;++i)
		printf("Entry: %s\n", ftw_result_store.data[i]);
	DynamicStringArrayDeinit(&ftw_result_store);

	const char* message = "hello, omg it's c time\n";

	unsigned char hash[SHA_DIGEST_LENGTH];
	CalculateSHA1Hash(message, strlen(message), hash);
	printf("%s\n", message);
	for(int i=0; i<SHA_DIGEST_LENGTH; ++i)
	{
		printf("%x", hash[i]);
	}
	printf("\n");

	struct ProjectDescription description;
	const char* json_string	 = "{\"executable_name\": \"LightSpeedFileExplorer\", \"link_dependencies_for_executable\": [\"freetype.so\", \"libpng.so\"]}";
	if(!ProjectDescriptionFromJSON(&description, json_string)){
		fprintf(stderr, "Something went wrong parsing json\n");
		return 1;
	}
	printf("Project description\nExecutable=%s\nDependencies=[", description.executable_name);
	if(description.link_dependencies_for_executable.size>0)
		printf("%s,",description.link_dependencies_for_executable.data[0]);
	for(int i=1; i<description.link_dependencies_for_executable.size; ++i){
		printf("%s",description.link_dependencies_for_executable.data[i]);
	}
	printf("]\n");
	ProjectDescriptionDeinit(&description);
	return 0;
}
