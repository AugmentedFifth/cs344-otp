#include <stdio.h> // FILE


#define FILE_CHUNK_SIZE 100


// Forward declarations
int send_contents(char* argv0, FILE* fh, int socket_fd);

int handle_args(int    argc,
                char** argv,
                int*   port_num_,
                FILE** plaintext_,
                FILE** key_);
