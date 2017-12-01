#pragma once

/*** Macro constants ***/
#define RECV_CHUNK_SIZE 100
#define MAX_CONNECTIONS 5
#define MAGIC_HANDSHAKE "encode!"


/*** Forward declarations ***/

// Takes an integer value and returns the corresponding character of the
// cypher based on the residue of that value, mod 27.
//
// ## Parameters:
// * `val` - The value of the character in the cypher.
//
// **Returns** the corresponding character in the cypher.
char char_of_val(int val);

// Takes a cypher character and returns the corresponding integer value
// (mod 27) according to the cypher.
//
// ## Parameters:
// * `c` - A cypher character (matches the regex `/[A-Z ]/`).
//
// **Returns** the corresponding value (in the range [0, 26]) according to
// the cypher.
int val_of_char(char c);

// Encodes the given plaintext using the one-time pad method, given the key
// provided. *The plaintext is overwritten*.
//
// ## Parameters:
// * `plaintext` - *Both an input and output parameter*, which comes in as the
//                 plaintext to be encoded, and out as the encoded text.
// * `text_len` - String-length of the `plaintext`.
// * `key` - Key string, which must be at least as long as `plaintext`.
void encode(char* plaintext, int text_len, const char* key);

// Goes through current pool of child processes, clearing out (waiting for)
// processes that are finished and counting up how many slots are now open in
// total.
//
// **Returns** the number of open slots for child processes, which must always
// be in the range [0, `MAX_CONNECTIONS`].
int check_on_children(void);

// Sends a `SIGTERM` to all child processes.
void kill_children(void);

// Writes a string to the specified socket.
//
// Does *not* ensure that all of the data is actually emitted from the kernel's
// (ioctl) send buffer upon returning from this function.
//
// ## Parameters:
// * `conn_fd` - The file descriptor of the socket to write to.
// * `msg` - The string to write.
// * `msg_len` - The length of the message (typically `strlen(msg)`).
//
// **Returns** -1 on failure, 0 on success.
int send_msg(int conn_fd, const char* msg, int msg_len);

// Receives data (up to `buf_size - 1` bytes) from the specified socket into
// the given buffer.
//
// ## Parameters:
// * `conn_fd` - The file descriptor of the socket to write to.
// * `buf` - (*Output parameter*) The buffer to write the received bytes to.
// * `buf_size` - The size of the buffer, in bytes.
//
// **Returns** -1 on failure, 0 on success.
int do_recv(int conn_fd, char* buf, int buf_size);

// Performs the "handshake" procedure with a client, checking that the client
// sends the right magic handshake data and then acknowledging or declining
// as appropriate. Performs `shutdown()` and `close()` as necessary.
//
// ## Parameters:
// * `conn_fd` - The file descriptor of the socket that the client connection
//               is using.
//
// **Returns** negative on failure, 0 on success.
int handshake(int conn_fd);
