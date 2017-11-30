keygen: keygen.h keygen.c
	gcc -o keygen keygen.c -O -g -ftrapv -Wall -Wextra -Wshadow -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -Winit-self

otp_enc: otp_enc.h otp_enc.c
	gcc -o otp_enc otp_enc.c -O -g -ftrapv -Wall -Wextra -Wshadow -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -Winit-self

otp_enc_d: otp_enc_d.h otp_enc_d.c
	gcc -o otp_enc_d otp_enc_d.c -O -g -ftrapv -Wall -Wextra -Wshadow -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -Winit-self
