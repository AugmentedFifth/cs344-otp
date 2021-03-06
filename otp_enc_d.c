#include "otp_enc_d.h"

#include <netinet/in.h> // sockaddr_in, htons, sockaddr
#include <stdio.h>      // fprintf
#include <stdlib.h>     // atoi
#include <string.h>     // memcpy, memset, strchr, strncpy
#include <sys/socket.h> // AF_INET, socket, SOCK_STREAM, connect, send, recv
#include <sys/types.h>  // pid_t
#include <sys/wait.h>   // waitpid
#include <unistd.h>     // fork


/*** Ghoulish, Ghastly Globals ***/
pid_t children[MAX_CONNECTIONS] = {0};


/*** Implementations ***/

char char_of_val(int val)
{
    val %= 27;
    if (val < 0) // Catch negative values of `val` giving negative remainders
    {
        val += 27;
    }

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

        // Overwriting plaintext buffer
        plaintext[i] = char_of_val(text_val + key_val);
    }
}

int check_on_children(void)
{
    int empty_slots = 0;
    int i;
    for (i = 0; i < MAX_CONNECTIONS; ++i)
    {
        if (children[i] == 0)
        {
            empty_slots++; // Slot is already empty
        }
        else
        {
            int wstatus;
            // Try to gather up the finished child process, but don't wait for
            // it if it's still going
            pid_t waited = waitpid(children[i], &wstatus, WNOHANG);
            switch (waited)
            {
                case -1: // Yikes
                {
                    perror("otp_enc_d ERROR in waitpid()");
                    return -1;
                }
                case 0:  // It's still going, so we leave it alone
                {
                    break;
                }
                default: // Wasn't empty before, but we joined it and now it's
                {        // a clear spot
                    children[i] = 0;
                    empty_slots++;
                    break;
                }
            }
        }
    }

    return empty_slots;
}

void kill_children(void)
{
    int i;
    for (i = 0; i < MAX_CONNECTIONS; ++i)
    {
        if (children[i] != 0)
        {
            kill(children[i], SIGTERM);
        }
    }
}

int send_msg(int conn_fd, const char* msg, int msg_len)
{
    int total_sent = 0;
    while (total_sent < msg_len) // Keep sending until the entire message
    {                            // is in the send buffer
        int chars_sent = send(
            conn_fd,
            &msg[total_sent],
            msg_len - total_sent,
            0
        );
        if (chars_sent < 0) // D'oh
        {
            perror("otp_enc_d ERROR writing to socket");
            close(conn_fd);
            return 1;
        }

        total_sent += chars_sent;
    }

    return 0;
}

int do_recv(int conn_fd, char* buf, int buf_size)
{
    int chars_recved = recv(conn_fd, buf, buf_size - 1, 0);
    if (chars_recved < 0) // !!!
    {
        perror("otp_enc_d ERROR reading from socket");
        close(conn_fd);
        return -1;
    }

    return chars_recved;
}

int handshake(int conn_fd)
{
    // Check that the client sends exactly `MAGIC_HANDSHAKE`
    int seen_magic = 0;
    int magic_ix = 0;
    while (!seen_magic)
    {
        char recv_buf[RECV_CHUNK_SIZE] = {0};
        int handshake_char_count = do_recv(conn_fd, recv_buf, RECV_CHUNK_SIZE);
        if (handshake_char_count < 0) // Catastrophe
        {
            return -1;
        }

        int i;
        for (i = 0; i < handshake_char_count; ++i)
        {
            if (
                magic_ix + i >= strlen(MAGIC_HANDSHAKE) ||
                recv_buf[i] != MAGIC_HANDSHAKE[magic_ix + i]
            ) {
                // Send rejection message
                if (send_msg(conn_fd, "?", 1) < 0)
                {
                    return -1;
                }

                if (shutdown(conn_fd, SHUT_RDWR) < 0)
                {
                    perror("otp_enc_d ERROR shutting down socket connection");
                }
                close(conn_fd);
                return -1;
            }

            if (recv_buf[i] == MAGIC_HANDSHAKE[strlen(MAGIC_HANDSHAKE) - 1])
            {
                seen_magic = 1;
                break;
            }
        }

        magic_ix = i;
    }

    // Acknowledge
    int send_res = send_msg(conn_fd, ".", 1);
    if (send_res < 0)
    {
        return send_res;
    }

    return 0;
}

int handle_client(int est_conn_fd)
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

    // Initial handshake to confirm that we are getting a connection
    // from otp_enc
    int handshake_res = handshake(est_conn_fd);
    if (handshake_res < 0)
    {
        free(key);
        free(plaintext);
        return 1;
    }

    // Start receiving body of message.
    // We use these boolean (`int`) variables as flags to indicate how "far"
    // we've read into the client's message. Message sections are ended by the
    // `'\n'` character
    int recving_plaintext = 1;
    int recving_key       = 1;
    while (recving_plaintext)
    {
        memset(recv_buf, '\0', RECV_CHUNK_SIZE);

        int chars_recved = do_recv(
            est_conn_fd,
            recv_buf,
            RECV_CHUNK_SIZE
        );
        if (chars_recved < 0)
        {
            free(key);
            free(plaintext);
            return 1;
        }

        if (chars_recved == 0)
        {
            recving_plaintext = 0;
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

            char* snd_newline_loc = strchr(newline_loc + 1, '\n');

            key_len += chars_recved - chars_to_cpy - 1;
            strncpy(key, newline_loc + 1, key_len);
            key[key_len] = '\0';

            recving_plaintext = 0;
            if (snd_newline_loc != NULL)
            {
                recving_key = 0;
            }
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

    // Similar logic to the above loop
    while (recving_key)
    {
        memset(recv_buf, '\0', RECV_CHUNK_SIZE);

        int chars_recved = do_recv(
            est_conn_fd,
            recv_buf,
            RECV_CHUNK_SIZE
        );
        if (chars_recved < 0)
        {
            free(key);
            free(plaintext);
            return 1;
        }

        if (chars_recved == 0)
        {
            recving_key = 0;
        }

        char* newline_loc = strchr(recv_buf, '\n');
        int chars_to_cpy = chars_recved;
        if (newline_loc != NULL)
        {
            // If we see the newline signifying the end of the key
            // segment, then we want to get only the part leading
            // up to the newline and then stop reading altogether
            chars_to_cpy = newline_loc - recv_buf;

            recving_key = 0;
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
    }

    // Check to make sure we have enough key
    if (key_len < plaintext_len)
    {
        fprintf(
            stderr,
            "otp_enc_d ERROR: key is shorter than plain text portion\n"
        );
        free(key);
        free(plaintext);
        close(est_conn_fd);
        return 1;
    }

    // Do the encoding, overwriting the `plaintext` buffer
    encode(plaintext, plaintext_len, key);

    // Send back the encoded message
    int send_res = send_msg(est_conn_fd, plaintext, plaintext_len);
    if (send_res != 0)
    {
        free(key);
        free(plaintext);
        return send_res;
    }

    // Child cleanup
    free(key);
    free(plaintext);
    if (shutdown(est_conn_fd, SHUT_RDWR) < 0)
    {
        perror("otp_enc_d ERROR shutting down socket connection");
        close(est_conn_fd);
        return 1;
    }
    close(est_conn_fd);

    return 0;
}

int server_loop(int socket_fd)
{
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_info_size = sizeof(client_addr);
        int est_conn_fd = accept( // Obtain a connection
            socket_fd,
            (struct sockaddr*)&client_addr,
            &client_info_size
        );
        if (est_conn_fd < 0)
        {
            perror("otp_enc_d ERROR on accept");
            return 1;
        }

        int empty_slots = check_on_children();
        if (empty_slots == 0)   // Immediately close socket, rejecting client
        {                       // because we already have the max number of
            close(est_conn_fd); // clients connected
            continue;
        }
        else if (empty_slots < 0) // Something is terribly wrong
        {
            close(socket_fd);
            close(est_conn_fd);
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
            exit(handle_client(est_conn_fd));
        }
        else // Parent process
        {
            // Register child process with the global pool
            int i;
            for (i = 0; i < MAX_CONNECTIONS; ++i)
            {
                if (children[i] == 0)
                {
                    children[i] = spawned_pid;
                    break;
                }
            }
        }
    }

    return 0;
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
        fprintf(stderr, "listening_port must be a positive integer\n");
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
        perror("otp_enc_d ERROR opening socket");
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
        perror("otp_enc_d ERROR on binding socket");
        return 1;
    }
    listen(socket_fd, 5);

    // Start accepting connections
    int ret = server_loop(socket_fd);

    // Cleanup
    kill_children();
    if (shutdown(socket_fd, SHUT_RDWR) < 0)
    {
        perror("otp_enc_d ERROR shutting down main socket connection");
        ret = 1;
    }
    close(socket_fd);

    return ret;
}
