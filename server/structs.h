/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


typedef enum
{
	HB_RECV = 0,
	S_SENT,
	APPROVED,
	DENIED
} server_state;

typedef struct _servers_s
{
	DWORD				last_access;
	DWORD				hb_time;
	DWORD				ss_time;
	unsigned short		q_port;
	struct sockaddr_in	address;
	server_state		state;
	int					ssent;
	BOOL				show;
    struct _servers_s	*next;
	struct _servers_s	*prev;
} servers_s;

typedef struct _filters_s
{
	int				augsperkill;
	int				augsperstart;
} filters_s;

typedef struct _config_msm_s
{
	unsigned char		*motd_file;
	DWORD				motd_interval;
	unsigned short		listen_port;
	unsigned char		motd_lines_host[MAX_MOTD_LINES][MAX_MOTD_HOST_SIZE];
	unsigned char		motd_lines_map[MAX_MOTD_LINES][MAX_MOTD_MAP_SIZE];
	unsigned char		motd_lines_gametype[MAX_MOTD_LINES][MAX_MOTD_GAMETYPE_SIZE];
	int					lines;
	unsigned char		ip[16];
} config_msm_s;

typedef struct _config_s
{
	unsigned char		*log_file;
	unsigned char		*inc_file;
	unsigned char		*exc_file;
	unsigned char		*ban_file;
	int					sleep_time;
	unsigned short		udp_port;
	unsigned short		tcp_port;
	unsigned char		game_name[32];
	DWORD				server_timeout;
	DWORD				server_status_timeout;
	int					max_resend_packets;
	DWORD				resend_delay;
	int					client_timeout;
	filters_s			filters;
	BOOL				debug_mode;
	config_msm_s		msm;
} config_s;

// motd session mod = MSM
typedef struct _msm_clients_s
{
	struct sockaddr_in		address;
	DWORD					motd_shown;
	struct _msm_clients_s	*next;
	struct _msm_clients_s	*prev;
} msm_clients_s;

typedef struct _server_data_s
{
	SOCKET				sock;
	config_s			cfg;
	struct sockaddr_in	from;
	unsigned char		data[1024];
	int					len;
	unsigned short		q_port;
	servers_s			*servers;
	DWORD				current_time;
	DWORD				include_time;
	msm_clients_s		*msm;
	CRITICAL_SECTION	cs;
} server_data_s;