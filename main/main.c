#include <stdio.h>
#include "sds.h"
int main()
{
    sds cmd = sdsnew("hap");
    cmd = sdsgrowzero(cmd, 5);
    struct sdshdr *sh = SDS_2_SDSHDR(cmd);
    return 0;
}