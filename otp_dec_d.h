// Macro constants
#define RECV_CHUNK_SIZE 100
#define MAX_CONNECTIONS 5
#define MAGIC_HANDSHAKE "encode!"


// Forward declarations
char char_of_val(int val);

int val_of_char(char c);

void encode(char* plaintext, int text_len, const char* key);

int check_on_children(void);

void kill_children(void);

int send_msg(int conn_fd, const char* msg, int msg_len);

int do_recv(int conn_fd, char* buf, int buf_size);

int handshake(int conn_fd);
