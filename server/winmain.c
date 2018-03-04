/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "structs.h"
#include "log.h"
#include "servers.h"
#include "clients.h"
#include "motd.h"

static WSADATA	winsockdata;

unsigned char		log_file[MAX_PATH] = {0};
unsigned char		inc_file[MAX_PATH] = {0};
unsigned char		exc_file[MAX_PATH] = {0};
unsigned char		ban_file[MAX_PATH] = {0};
unsigned char		motd_file[MAX_PATH] = {0};


void SetConfig(config_s *cfg, char *value, char *data)
{
	if (value[0] == ';')
		return;

	if (!strcmp(value, "logfile"))
	{
		if (!strcmp(data, "0"))
			cfg->log_file = NULL;
		else
		{
			strncpy(log_file, data, sizeof(log_file) - 1);
			cfg->log_file = log_file;
		}
	}

	else if (!strcmp(value, "include"))
	{
		strncpy(inc_file, data, sizeof(inc_file) - 1);
		cfg->inc_file = inc_file;
	}

	else if (!strcmp(value, "exclude"))
	{
		strncpy(exc_file, data, sizeof(exc_file) - 1);
		cfg->exc_file = exc_file;
	}

	else if (!strcmp(value, "bansfile"))
	{
		strncpy(ban_file, data, sizeof(ban_file) - 1);
		cfg->ban_file = ban_file;
	}

	else if (!strcmp(value, "motdfile"))
	{
		strncpy(motd_file, data, sizeof(motd_file) - 1);
		cfg->msm.motd_file = motd_file;
	}

	else if (!strcmp(value, "motdip"))
	{
		strncpy(cfg->msm.ip, data, sizeof(cfg->msm.ip) - 1);
	}

	else if (!strcmp(value, "motdport"))
	{
		cfg->msm.listen_port = atoi(data);
	}

	else if (!strcmp(value, "motdinterval"))
	{
		cfg->msm.motd_interval = atoi(data) * 1000;
	}

	else if (!strcmp(value, "sleeptime"))
	{
		cfg->sleep_time = atoi(data);
	}

	else if (!strcmp(value, "udpport"))
	{
		cfg->udp_port = atoi(data);
	}

	else if (!strcmp(value, "servertimeout"))
	{
		cfg->server_timeout = atoi(data);
	}

	else if (!strcmp(value, "statustimeout"))
	{
		cfg->server_status_timeout = atoi(data);
	}

	else if (!strcmp(value, "maxstatuspackets"))
	{
		cfg->max_resend_packets = atoi(data);
	}

	else if (!strcmp(value, "statusresenddelay"))
	{
		cfg->resend_delay = atoi(data);
	}

	else if (!strcmp(value, "gamename"))
	{
		strncpy(cfg->game_name, data, sizeof(cfg->game_name) - 1);
	}

	else if (!strcmp(value, "tcpport"))
	{
		cfg->tcp_port = atoi(data);
	}

	else if (!strcmp(value, "clienttimeout"))
	{
		cfg->client_timeout = atoi(data);
	}

	else if (!strcmp(value, "filter_augsperkill"))
	{
		cfg->filters.augsperkill = atoi(data);
	}

	else if (!strcmp(value, "filter_augsperstart"))
	{
		cfg->filters.augsperstart = atoi(data);
	}

	else if (!strcmp(value, "debug"))
	{
		if (data[0] == '1')
			cfg->debug_mode = TRUE;
	}
}


void SetDefaults(config_s *cfg)
{
	cfg->sleep_time = DEFAULT_SLEEP_TIME;
	cfg->udp_port = DEFAULT_UDP_PORT;
	cfg->server_timeout = DEFAULT_SERVER_TIMEOUT;
	cfg->server_status_timeout = DEFAULT_SERVER_S_TIMEOUT;
	cfg->max_resend_packets = DEFAULT_MAX_RESEND_PACKETS;
	cfg->resend_delay = DEFAULT_RESEND_DELAY;
	strncpy(cfg->game_name, DEFAULT_GAME_NAME, sizeof(cfg->game_name) - 1);
	cfg->tcp_port = DEFAULT_TCP_PORT;
	cfg->client_timeout = DEFAULT_CLIENT_TIMEOUT;
	cfg->msm.motd_interval = DEFAULT_MOTD_INTERVAL;
	cfg->msm.listen_port = DEFAULT_MOTD_PORT;
}


BOOL ReadConfig(config_s *cfg)
{
	FILE		*f;
	char		c, 
				value[MAX_ORDER_SIZE] = {0}, 
				data[MAX_ORDER_SIZE] = {0};
	int			i = 0, 
				k = 0;

	f = fopen(DEFAULT_CONFIG_FILE, "rb");
	if (f == NULL)
	{
		Print(cfg, MSG_WARNING, "Failed to open config file. Using default config values.");
		return FALSE;
	}

	while ((c = fgetc(f)) != EOF)
	{
		if (c == ' ')
		{
			k = 1;
			i = 0;
			memset(data, 0, sizeof(data));

			continue;
		}
		else if (c == '\n' || c == '\r')
		{
			k = 0;
			i = 0;

			// parse
			SetConfig(cfg, value, data);
			memset(value, 0, sizeof(value));
			memset(data, 0, sizeof(data));

			continue;
		}

		if (!k)
			value[i] = c;
		else
			data[i] = c;

		i++;
	}

	SetConfig(cfg, value, data);

	fclose(f);

	return TRUE;
}


BOOL WSAInitialize(config_s *cfg)
{
	int		r;

	r = WSAStartup(MAKEWORD(2, 2),&winsockdata);

	if (r) 
	{
		Print(cfg, MSG_ERROR, "Winsock initialization failed, returned %d.", r);
		return FALSE;
	}
	else
	{
		Print(cfg, MSG_SYSTEM, "Winsock initialized.");
		return TRUE;
	}
}


int main(int argc, char *argv[]) 
{
	server_data_s	server_data;

	printf("Unreal MasterServer emulator v1.0 by DejaVu\n");
	printf("i1337bb@hotmail.com\n\n");

	memset(&server_data, 0, sizeof(server_data));

	SetDefaults(&server_data.cfg);
	ReadConfig(&server_data.cfg);

	LOG_Initialize(&server_data.cfg);

	if (server_data.cfg.debug_mode)
		Print(&server_data.cfg, MSG_SYSTEM, "Debug mode on");

	Print(&server_data.cfg, MSG_SYSTEM, "Initializing ctritical section");
	InitializeCriticalSection(&server_data.cs);

	if (!WSAInitialize(&server_data.cfg))
		return 0;

	// create thread for motd
	MOTD_Start(&server_data);

	Sleep(100);

	// create another thread for clients
	if (!CL_Start(&server_data))
		return 0;

	Sleep(100);

	// main loop for listening to servers heartbeats
	SRV_Main(&server_data);

	// never reached
}