#include "cfd_core.h"
#include <stdlib.h>
void test_success(void);
void test_oom(u64 requested);
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    test_success();
    test_oom((u64)strtoull(argv[1], NULL, 10));
    return 0;
}
