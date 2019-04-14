// TCP backend for SMeCoL

// Options
typedef struct irc_opts {
	char* nick;
	char* server;
	int port;
} irc_opts;

#define IRC_OPTS(n, s, p) ((tcp_opts) { .nick = (n), .server = (s), .port = (p) })

int smecol_irc_register(void);
