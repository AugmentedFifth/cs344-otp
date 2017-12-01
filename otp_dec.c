#include "otp_dec.h"

#include <netdb.h>      // hostent, gethostbyname
#include <netinet/in.h> // sockaddr_in, htons, sockaddr
#include <stdio.h>      // fprintf, NULL, FILE, fopen, fclose, perror, fileno
#include <stdlib.h>     // atoi
#include <string.h>     // memcpy, memset, strchr, strlen
#include <sys/socket.h> // AF_INET, socket, SOCK_STREAM, connect, send, recv
#include <sys/stat.h>   // fstat
#include <unistd.h>     // close


/*** Implementations ***/

int send_contents(FILE* fh, int socket_fd)
{
    char file_buf[FILE_CHUNK_SIZE]; // Buffer to read the file into
    while (fgets(file_buf, FILE_CHUNK_SIZE, fh) != NULL) // Read
    {
        // Check that this chunk of the file is ready to be sent
        int i = 0;
        while (file_buf[i] != '\0')
        {
            if (file_buf[i] == '\n') // That's all for this file
            {
                file_buf[i + 1] = '\0'; // Just making sure we don't send
                                        // anything beyond the delimiter
            }
            else if (
                ('A' > file_buf[i] || 'Z' < file_buf[i]) && // Bad character
                file_buf[i] != ' '
            ) {
                fprintf(
                    stderr,
                    "otp_dec ERROR: unrecognized character '%c'\n",
                    file_buf[i]
                );
                return 1;
            }

            i++;
        }

        // Send the chunk
        if (send_msg(socket_fd, file_buf, i) < 0)
        {
            return 1;
        }
    }

    return 0;
}

int handle_args(int          argc,
                char* const* argv,
                int*         port_num_,
                FILE**       plaintext_,
                FILE**       key_)
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
        fprintf(stderr, "port must be a positive integer\n");
        return 1;
    }

    // Check that `<plaintext>` and `<key>` can be read and that `<key>` is
    // at least as long as `<plaintext>`
    FILE* plaintext = fopen(argv[1], "r");
    if (plaintext == NULL)
    {
        perror("otp_dec ERROR opening plaintext file for reading");
        return 1;
    }
    FILE* key = fopen(argv[2], "r");
    if (key == NULL)
    {
        perror("otp_dec ERROR opening key file for reading");
        return 1;
    }

    struct stat stat_buf;
    if (fstat(fileno(plaintext), &stat_buf) < 0)
    {
        perror("otp_dec ERROR stating plaintext file");
        return 1;
    }
    off_t plaintext_size = stat_buf.st_size;
    if (fstat(fileno(key), &stat_buf) < 0)
    {
        perror("otp_dec ERROR stating key file");
        return 1;
    }
    if (stat_buf.st_size < plaintext_size)
    {
        fprintf(
            stderr,
            "otp_dec ERROR: the key must be at least as long as the message\n"
        );
        return 1;
    }

    // Write relevant obtained values to ouput parameters
    *port_num_  = port_num;
    *plaintext_ = plaintext;
    *key_       = key;

    return 0;
}

int send_msg(int conn_fd, const char* msg, int msg_len)
{
    int total_written = 0;
    while (total_written < msg_len) // Keep sending until the entire message
    {                               // is in the send buffer
        int chars_written = send(
            conn_fd,
            &msg[total_written],
            msg_len - total_written,
            0
        );
        if (chars_written < 0) // Yeesh
        {
            perror("otp_dec ERROR writing to socket");
            return -1;
        }

        total_written += chars_written;
    }

    return 0;
}

int main(int argc, char** argv)
{
    // Handle the arguments passed into the program
    int port_num;
    FILE* plaintext;
    FILE* key;
    int args_res = handle_args(argc, argv, &port_num, &plaintext, &key);
    if (args_res != 0)
    {
        return args_res;
    }

    // Clear and fill the server address struct
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    // Hardcoding `"localhost"` since it's not specified as an argument
    struct hostent* server_host_info = gethostbyname(HOST_NAME);
    if (server_host_info == NULL)
    {
        fprintf(stderr, "otp_dec ERROR: no such host %s\n", HOST_NAME);
        fclose(key);
        fclose(plaintext);
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
        perror("otp_dec ERROR opening socket");
        fclose(key);
        fclose(plaintext);
        return 1;
    }

    // Connect to otp_dec_d
    if (
        connect(
            socket_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)
        ) < 0
    ) {
        perror("otp_dec ERROR connecting to otp_dec_d");
        fclose(key);
        fclose(plaintext);
        return 2;
    }

    // Send handshake
    if (send_msg(socket_fd, MAGIC_HANDSHAKE, strlen(MAGIC_HANDSHAKE)) < 0)
    {
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return 1;
    }

    // Look for acknowledgement
    char recv_buf[FILE_CHUNK_SIZE] = {0};
    int ack_len = recv(socket_fd, recv_buf, sizeof(recv_buf) - 1, 0);
    if (ack_len < 0)
    {
        perror("otp_dec ERROR receiving acknowledgement");
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return 1;
    }
    if (recv_buf[0] != ACK_CHAR)
    {
        fprintf(stderr, "otp_dec ERROR: rejected by server!\n");
        fprintf(
            stderr,
            "(you probably connected to something that wasn't otp_dec_d)\n"
        );
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return 1;
    }

    // Read "chunks" of the `plaintext` file, sending each one sequentially
    // after checking that all the characters are valid
    int send_res = send_contents(plaintext, socket_fd);
    if (send_res != 0)
    {
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return send_res;
    }
    // Do the same for `key`
    send_res = send_contents(key, socket_fd);
    if (send_res != 0)
    {
        close(socket_fd);
        fclose(key);
        fclose(plaintext);
        return send_res;
    }

    // Slorp up the data we get back and output directly to `stdout`
    int chars_recved;
    do
    {
        memset(recv_buf, '\0', sizeof(recv_buf));

        chars_recved = recv(socket_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (chars_recved < 0)
        {
            perror("otp_dec ERROR reading from socket");
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

    write(STDOUT_FILENO, "\n", 1);
    fflush(stdout);

    // Cleanup
    fclose(key);
    fclose(plaintext);
    if (shutdown(socket_fd, SHUT_RDWR) < 0)
    {
        perror("otp_dec ERROR shutting down socket connection");
        close(socket_fd);
        return 1;
    }
    close(socket_fd);

    return 0;
}
