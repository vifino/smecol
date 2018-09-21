// TCP backend for SMeCoL

// Settings:
#define SV_QUEUE 32

// Option struct. Yay.
typedef struct tcp_opts {
	int is_server;
	char* name; // for now.
	char* port;
} tcp_opts;

#define TCP_OPTS(s, i, p) ((tcp_opts) { .is_server = (s), .ip = (i), .port = (p) })


int smecol_tcp_register(void);
