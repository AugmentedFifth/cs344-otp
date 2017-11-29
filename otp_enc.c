#include "otp_enc.h"

#include <netdb.h>      // hostent, gethostbyname
#include <netinet/in.h> // sockaddr_in, htons, sockaddr
#include <stdio.h>      // fprintf, NULL, FILE, fopen, fclose, perror, fileno
#include <stdlib.h>     // atoi
#include <string.h>     // memcpy, memset, strchr, strlen
#include <sys/socket.h> // AF_INET, socket, SOCK_STREAM, connect, send, recv
#include <sys/stat.h>   // fstat
#include <unistd.h>     // close


int send_contents(char* argv0, FILE* fh, int socket_fd)
{
    char file_buf[FILE_CHUNK_SIZE];
    while (fgets(file_buf, FILE_CHUNK_SIZE, fh) != NULL)
    {
        // Check that this chunk of the file is ready to be sent
        int i = 0;
        while (file_buf[i] != '\0')
        {
            if (file_buf[i] == '\n')
            {
                file_buf[i + 1] = '\0'; // Just making sure we don't send
                                        // anything beyond the delimiter
            }
            else if ('A' > file_buf[i] || 'Z' < file_buf[i])
            {
                fprintf(
                    stderr,
                    "%s: ERROR; unrecognized character '%c'.\n",
                    argv0,
                    file_buf[i]
                );
                return 1;
            }

            i++;
        }

        // Send the chunk
        int total_written = 0;
        while (total_written < i)
        {
            int chars_written = send(
                socket_fd,
                &file_buf[total_written],
                i - total_written,
                0
            );
            if (chars_written < 0)
            {
                fprintf(stderr, "%s: ERROR writing to socket.\n", argv0);
                return 1;
            }

            total_written += chars_written;
        }
    }

    return 0;
}

int handle_args(int    argc,
                char** argv,
                int*   port_num_,
                FILE** plaintext_,
                FILE** key_)
{
    // Check usage and parse `<port>`
    if (argc != 4)
    {
        fprintf(stderr, "USAGE: %s <plaintext> <key> <port>\n", argv[0]);
        return 1;
    }

    int port_num = atoi(argv[3]);
    if (port_num < 1)
    {
        fprintf(stderr, "port must be a positive integer.\n");
        return 1;
    }

    // Check that `<plaintext>` and `<key>` can be read and that `<key>` is
    // at least as long as `<plaintext>`
    FILE* plaintext = fopen(argv[1], "r");
    if (plaintext == NULL)
    {
        perror("Could not open plaintext file for reading");
        return 1;
    }
    FILE* key = fopen(argv[2], "r");
    if (key == NULL)
    {
        perror("Could not open key file for reading");
        return 1;
    }

    struct stat stat_buf;
    if (fstat(fileno(plaintext), &stat_buf) < 0)
    {
        perror("Could not stat plaintext file");
        return 1;
    }
    off_t plaintext_size = stat_buf.st_size;
    if (fstat(fileno(key), &stat_buf) < 0)
    {
        perror("Could not stat key file");
        return 1;
    }
    if (stat_buf.st_size < plaintext_size)
    {
        fprintf(stderr, "The key must be at least as long as the message.\n");
        return 1;
    }

    *port_num_  = port_num;
    *plaintext_ = plaintext;
    *key_       = key;

    return 0;
}

int main(int argc, char** argv)
{
    int port_num;
    FILE* plaintext;
    FILE* key;
    handle_args(argc, argv, &port_num, &plaintext, &key);

    // Clear and fill the server address struct
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port_num);
    // Hardcoding "localhost" since it's not specified as an argument
    struct hostent* server_host_info = gethostbyname("localhost");
    if (server_host_info == NULL)
    {
        fprintf(stderr, "%s: ERROR, no such host %s.\n", argv[0], "localhost");
        return 1;
    }
    memcpy(
        (char*)&server_addr.sin_addr.s_addr,
        (char*)server_host_info->h_addr,
        server_host_info->h_length
    );

    // Set up the socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        fprintf(stderr, "%s: ERROR opening socket.\n", argv[0]);
        return 1;
    }

    // Connect to otp_enc_d
    if (
        connect(
            socket_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) < 0
    ) {
        fprintf(
            stderr,
            "%s: ERROR connecting on port %d.\n",
            argv[0],
            port_num
        );
        return 2;
    }

    // Read "chunks" of the `plaintext` file, sending each one sequentially
    // after checking that all the characters are valid
    int send_res = send_contents(argv[0], plaintext, socket_fd);
    if (send_res != 0)
    {
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return send_res;
    }
    // Do the same for `key`
    send_res = send_contents(argv[0], key, socket_fd);
    if (send_res != 0)
    {
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return send_res;
    }

    // Slorp up the data we get back and output directly to `stdout`
    char recv_buf[FILE_CHUNK_SIZE];
    int chars_recved;
    do
    {
        memset(recv_buf, '\0', sizeof(recv_buf));

        chars_recved = recv(socket_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (chars_recved < 0)
        {
            fprintf(stderr, "%s: ERROR reading from socket.\n", argv[0]);
            close(socket_fd);
            fclose(key);
            fclose(plaintext);
            return 1;
        }
        if (chars_recved == 0)
        {
            break;
        }

        char* newline_loc = strchr(recv_buf, '\n');
        if (newline_loc == NULL)
        {
            write(STDOUT_FILENO, recv_buf, strlen(recv_buf));
        }
        else
        {
            write(STDOUT_FILENO, recv_buf, newline_loc - recv_buf + 1);
            break;
        }
    } while (1);

    // Cleanup
    close(socket_fd);
    fclose(key);
    fclose(plaintext);

    return 0;
}