#define RECV_CHUNK_SIZE 100


// Forward declarations
char char_of_val(int val);

int val_of_char(char c);

void encode(char* plaintext, int text_len, const char* key);
