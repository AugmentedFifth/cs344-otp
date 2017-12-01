#pragma once

#include <stdio.h> // FILE


/*** Macro constants ***/
#define FILE_CHUNK_SIZE 100
#define HOST_NAME "localhost"
#define MAGIC_HANDSHAKE "edoced!"
#define ACK_CHAR '.'


/*** Forward declarations ***/

// Sends the contents read from the file handle `fh` (*up to the first
// newline*) through the socket described by `socket_fd` in chunks of
// `FILE_CHUNK_SIZE` bytes.
//
// Does *not* ensure that all of the data is actually emitted from the kernel's
// (ioctl) send buffer upon returning from this function.
//
// ## Parameters:
// * `fh` - The file handle to read from and send contents of.
// * `socket_fd` - File descriptor of socket to send content through.
//
// **Returns** a non-zero exit code on failure, or zero on success.
int send_contents(FILE* fh, int socket_fd);

// Handles the arguments passed into the program, parsing the port number and
// opening up the relevant files for reading, checking that they are valid
// plain files that can be read from.
//
// ## Parameters:
// * `argc` - Number of arguments.
// * `argv` - The argument vector.
// * `port_num_` - This is an *output parameter*, into which the port number
//                 is written.
// * `plaintext_` - This is another output parameter, into which the file
//                  handle for the plaintext file is written.
// * `key_` - This is another output parameter, into which the file handle for
//            the key file is written.
//
// **Returns** a non-zero exit code on failure, or zero on success.
int handle_args(int          argc,
                char* const* argv,
                int*         port_num_,
                FILE**       plaintext_,
                FILE**       key_);

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
