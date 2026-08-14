#include <stdint.h>
#include <stddef.h>

typedef int (*addon_write_file_fn)(const char *path, const char *data, size_t len);

typedef struct {
    addon_write_file_fn writeFile;
} addon_api_t;

__attribute__((used)) void main(const addon_api_t *api) {
    if (!api || !api->writeFile) {
        return;
    }
    char path[10];
    path[0] = '/'; path[1] = 't'; path[2] = 'e'; path[3] = 's'; path[4] = 't';
    path[5] = '.'; path[6] = 't'; path[7] = 'x'; path[8] = 't'; path[9] = '\0';

    char data[18];
    data[0] = 'a'; data[1] = 'd'; data[2] = 'd'; data[3] = 'o'; data[4] = 'n';
    data[5] = ' '; data[6] = 'f'; data[7] = 'i'; data[8] = 'l'; data[9] = 'e';
    data[10] = ' '; data[11] = 't'; data[12] = 'e'; data[13] = 's'; data[14] = 't';
    data[15] = ' '; data[16] = '\n'; data[17] = '\0';

    api->writeFile(path, data, sizeof(data) - 1);
}
