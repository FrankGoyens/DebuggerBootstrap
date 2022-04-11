#include <stdio.h>

#include "dummy_dependency/dummy_dependency.h"

int main(int argc, char** argv){
    printf("This is the dummy project.\n");
    UseDummyDependency();
    return 0;
}