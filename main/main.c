#include <stdio.h>
#include "sds.h"
int main() {
    sds cmd = sdsnew("happysfkdsaflkjafl");
    printf("%s\n", cmd);
    return 0;
}