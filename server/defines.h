/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


#define DEFAULT_CONFIG_FILE			"config.cfg"
#define DEFAULT_SLEEP_TIME			50 // 20 checks per second
#define MAX_ORDER_SIZE				256

#define DEFAULT_UDP_PORT			27900
#define DEFAULT_SERVER_TIMEOUT		120000 // miliseconds
#define DEFAULT_SERVER_S_TIMEOUT	10000 // miliseconds
#define DEFAULT_MAX_RESEND_PACKETS  3
#define DEFAULT_RESEND_DELAY		3000 // miliseconds
#define DEFAULT_GAME_NAME			"deusex"
#define DEFAULT_TCP_PORT			28900
#define DEFAULT_CLIENT_TIMEOUT		5
#define DEFAULT_MOTD_INTERVAL		30000 // milisec
#define DEFAULT_MOTD_PORT			7000

#define STR_HEARTBEAT				"heartbeat"
#define STR_HEARTBEAT_LEN			9
#define STR_GAMENAME				"gamename"
#define STR_GAMENAME_LEN			8
#define STR_HOSTPORT				"hostport"
#define STR_HOSTPORT_LEN			8
#define STR_STATUS					"\\status\\"
#define STR_STATUS_LEN				8

#define STR_BASICSECURE				"\\basic\\\\secure\\ABCDEF\x0"
#define STR_BASICSECURE_LEN			23
#define STR_FINAL					"\\final\\"
#define STR_FINAL_LEN				7

#define MAX_PENDING_CONNECTIONS		8
#define MAX_RECV_BUFFER				1024
#define INCLUDE_REJOIN_TIME			60000 // miliseconds

#define CONSOLE_TITLE				"UMSE v1.0     Visible servers: %d     Pending servers: %d"

#define MAX_MOTD_LINES				16
#define MAX_MOTD_HOST_SIZE			64
#define MAX_MOTD_MAP_SIZE			32
#define MAX_MOTD_GAMETYPE_SIZE		16
