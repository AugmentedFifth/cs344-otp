#!/usr/bin/env python3

import sys


def char_of_val(val):
    if val == 26:
        return ' '
    else:
        return chr(ord('A') + val)

def val_of_char(c):
    if c == ' ':
        return 26
    else:
        return ord(c) - 65

def encode(plaintext, text_len, key):
    encoded = ""
    for i in range(text_len):
        text_val = val_of_char(plaintext[i])
        key_val = val_of_char(key[i])

        encoded += char_of_val((text_val + key_val) % 27)

    return encoded


if len(sys.argv) != 3:
    print("argc == " + str(len(sys.argv)))
    sys.exit(1)

plaintext = ""
with open(sys.argv[1], "r") as pt:
    plaintext = pt.read()

key = ""
with open(sys.argv[2], "r") as k:
    key = k.read()

encoded = encode(plaintext[:-1], len(plaintext) - 1, key[:-1])

print(encoded)
