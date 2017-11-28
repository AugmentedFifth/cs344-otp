#include "keygen.h"

#include <stdio.h>  // printf, NULL, fprintf
#include <stdlib.h> // srand, rand, atoi
#include <time.h>   // time


// Returns a random character that is a capital letter or a space.
// Uses `rand()`.
char random_char(void)
{
    int r = rand() % 27;

    if (r == 26)
    {
        return ' ';
    }
    
    return 'A' + r;
}

int main(int argc, char** argv)
{
    // Make sure the program was invoked with exactly one argument
    if (argc != 2)
    {
        fprintf(
            stderr,
            "Exactly one argument, the key length, must be specified.\n"
        );

        return 1;
    }

    // Initialize PRNG
    srand(time(NULL));

    // Get `keylength` and exit on invalid values
    int keylength = atoi(argv[1]);
    if (keylength < 1)
    {
        fprintf(stderr, "keylength must be a positive integer.\n");

        return 1;
    }

    // Create a buffer and fill it with the randomly generated key
    char buf[keylength + 2];
    int i;
    for (i = 0; i < keylength; ++i)
    {
        buf[i] = random_char();
    }
    buf[i] = '\n';
    buf[i + 1] = '\0';

    // Write the key to `stdout`
    printf(buf);

    return 0;
}
