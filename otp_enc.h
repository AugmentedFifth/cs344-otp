#include <stdio.h> // FILE


#define FILE_CHUNK_SIZE 100
#define HOST_NAME "localhost"


// Forward declarations
int send_contents(FILE* fh, int socket_fd);

int handle_args(int    argc,
                char** argv,
                int*   port_num_,
                FILE** plaintext_,
                FILE** key_);
