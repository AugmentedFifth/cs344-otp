#include "otp_enc_d.h"

#include <netinet/in.h> // sockaddr_in, htons, sockaddr
#include <stdio.h>      // fprintf
#include <stdlib.h>     // atoi
#include <string.h>     // memcpy, memset, strchr, strncpy
#include <sys/socket.h> // AF_INET, socket, SOCK_STREAM, connect, send, recv
#include <sys/types.h>  // pid_t
#include <unistd.h>     // fork


char char_of_val(int val)
{
    return val == 26 ? ' ' : 'A' + val;
}

int val_of_char(char c)
{
    return c == ' ' ? 26 : c - 65;
}

void encode(char* plaintext, int text_len, const char* key)
{
    int i;
    for (i = 0; i < text_len; ++i)
    {
        int text_val = val_of_char(plaintext[i]);
        int key_val  = val_of_char(key[i]);

        plaintext[i] = char_of_val((text_val + key_val) % 27);
    }
}

int main(int argc, char** argv)
{
    // Check usage and parse `<listening_port>`
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <listening_port>\n", argv[0]);
        return 1;
    }
    int listening_port = atoi(argv[1]);
    if (listening_port < 1)
    {
        fprintf(stderr, "listening_port must be a positive integer.\n");
        return 1;
    }

    // Clear and fill up address struct for this process
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(listening_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Set up the socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        fprintf(
            stderr,
            "%s: ERROR opening socket on port %d.\n",
            argv[0],
            listening_port
        );
        return 1;
    }

    // Enable the socket to start listening
    if (
        bind(
            socket_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) < 0
    ) {
        fprintf(
            stderr,
            "%s: ERROR on binding socket to port %d.\n",
            argv[0],
            listening_port
        );
        return 1;
    }
    listen(socket_fd, 5);

    // Start accepting connections
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_info_size = sizeof(client_addr);
        int est_conn_fd = accept(
            socket_fd,
            (struct sockaddr*)&client_addr,
            &client_info_size
        );
        if (est_conn_fd < 0)
        {
            fprintf(stderr, "%s: ERROR on accept.\n", argv[0]);
            return 1;
        }

        pid_t spawned_pid = fork();
        if (spawned_pid == -1)
        {
            perror("fork() failed!");
            close(socket_fd);
            close(est_conn_fd);
            return 1;
        }
        else if (spawned_pid == 0) // In the child process
        {
            // Get the plaintext content from the client, storing it in
            // dynamically allocated memory
            char recv_buf[RECV_CHUNK_SIZE];

            char* plaintext = malloc(RECV_CHUNK_SIZE * sizeof(char));
            int plaintext_cap = RECV_CHUNK_SIZE;
            int plaintext_len = 0;
            plaintext[0] = '\0';

            char* key = malloc(RECV_CHUNK_SIZE * sizeof(char));
            int key_cap = RECV_CHUNK_SIZE;
            int key_len = 0;
            key[0] = '\0';

            int recving_plaintext = 1;
            while (recving_plaintext)
            {
                memset(recv_buf, '\0', RECV_CHUNK_SIZE);

                int chars_recved = recv(
                    est_conn_fd,
                    recv_buf,
                    RECV_CHUNK_SIZE - 1,
                    0
                );
                if (chars_recved < 0)
                {
                    fprintf(
                        stderr,
                        "%s: ERROR reading from socket.\n",
                        argv[0]
                    );
                    close(est_conn_fd);
                    return 1;
                }

                char* newline_loc = strchr(recv_buf, '\n');
                int chars_to_cpy = chars_recved;
                if (newline_loc != NULL)
                {
                    // If we see the newline signifying the end of the plain
                    // text segment, then we want to get only the part leading
                    // up to the newline and potentially any trailing "key"
                    // data
                    chars_to_cpy = newline_loc - recv_buf;

                    key_len += chars_recved - chars_to_cpy - 1;
                    strncpy(key, newline_loc + 1, key_len);
                    key[key_len] = '\0';

                    recving_plaintext = 0;
                }
                // Allocate more space if necessary
                while (plaintext_len + chars_to_cpy > plaintext_cap - 1)
                {
                    plaintext_cap *= 2;
                    plaintext = realloc(
                        plaintext,
                        plaintext_cap * sizeof(char)
                    );
                }

                strncpy(
                    &plaintext[plaintext_len],
                    recv_buf,
                    chars_to_cpy
                );
                plaintext_len += chars_to_cpy;
                plaintext[plaintext_len] = '\0';
            }

            while (1)
            {
                memset(recv_buf, '\0', RECV_CHUNK_SIZE);

                int chars_recved = recv(
                    est_conn_fd,
                    recv_buf,
                    RECV_CHUNK_SIZE - 1,
                    0
                );
                if (chars_recved < 0)
                {
                    fprintf(
                        stderr,
                        "%s: ERROR reading from socket.\n",
                        argv[0]
                    );
                    close(est_conn_fd);
                    return 1;
                }

                char* newline_loc = strchr(recv_buf, '\n');
                int chars_to_cpy = chars_recved;
                int do_break = 0;
                if (newline_loc != NULL)
                {
                    // If we see the newline signifying the end of the key
                    // segment, then we want to get only the part leading
                    // up to the newline and then stop reading altogether
                    chars_to_cpy = newline_loc - recv_buf;

                    do_break = 1;
                }
                // Allocate more space if necessary
                while (key_len + chars_to_cpy > key_cap - 1)
                {
                    key_cap *= 2;
                    key = realloc(key, key_cap * sizeof(char));
                }

                strncpy(&key[key_len], recv_buf, chars_to_cpy);
                key_len += chars_to_cpy;
                key[key_len] = '\0';

                if (do_break)
                {
                    break;
                }
            }

            if (key_len < plaintext_len)
            {
                fprintf(
                    stderr,
                    "%s: ERROR; key is shorter than plain text portion.\n",
                    argv[0]
                );
                close(est_conn_fd);
                return 1;
            }

            // Child cleanup
            close(est_conn_fd);

            return 0;
        }
    }

    // Cleanup
    close(socket_fd);

    return 0;
}
