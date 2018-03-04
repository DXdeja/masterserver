#include <windows.h>
#include <stdio.h>

#include "defines.h"
#include "structs.h"
#include "log.h"


#define STR_STATUS		"\\status\\"
#define STR_INFO		"\\info\\"

typedef struct _motd_data_s
{
	SOCKET				sock[MAX_MOTD_LINES];
	server_data_s		*server_data;
	struct sockaddr_in	address;
	unsigned char		data[1024];
} motd_data_s;


int MOTD_InitializeSocket(SOCKET *sock, unsigned short port)
{
	struct sockaddr_in	address;
	unsigned long		mode = 1;
	int					i = 1;

	// Initialize new socket
	if ((*sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) 
		return 0;

	// Make it non-blocking
	if (ioctlsocket(*sock, FIONBIO, &mode) == SOCKET_ERROR) 
		return 0;

	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	address.sin_family = AF_INET;

	// Bind socket to UDP port
	if (bind(*sock, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) 
		return 0;

	return 1;
}


void MOTD_SendQueryData(motd_data_s *motd, int i)
{
	sprintf(motd->data, "\\gamename\\deusex\\gamever\\1100"
						"\\hostname\\%c|  %s\\hostport\\0\\"
						"mapname\\%s\\gametype\\%s\\numplayers\\0\\"
						"maxplayers\\0\\"
						"\\password\\False\\queryid\\1.1\\final\\",
						~(unsigned char)i,
						motd->server_data->cfg.msm.motd_lines_host[i],
						motd->server_data->cfg.msm.motd_lines_map[i],
						motd->server_data->cfg.msm.motd_lines_gametype[i]);

	sendto(motd->sock[i], 
		motd->data, 
		(int)strlen(motd->data), 
		0, 
		(struct sockaddr *)&motd->address, 
		sizeof(motd->address));		
	
	Print(&motd->server_data->cfg, MSG_DEBUG, 
			"MOTD: MOTD sent to %s:%d",
			inet_ntoa(motd->address.sin_addr),
			ntohs(motd->address.sin_port));

	return;
}


void MOTD_GetPacket(motd_data_s *motd)
{
	int				fsize = sizeof(motd->address);
	int				total;
	int				i;

	for (i = 0; i < motd->server_data->cfg.msm.lines; i++)
	{
		memset(motd->data, 0, sizeof(motd->data));

		total = recvfrom(motd->sock[i], 
						 motd->data, 
						 sizeof(motd->data) - 1, 
						 0, 
						 (struct sockaddr *)&motd->address, 
						 &fsize);

		if (total < -1)
			continue;
		else if (total < 6)
			continue;
		else
		{
			Print(&motd->server_data->cfg, MSG_DEBUG, 
				"MOTD: Received msg (len=%d) from %s:%d",
				total,
				inet_ntoa(motd->address.sin_addr),
				ntohs(motd->address.sin_port));
			Print(&motd->server_data->cfg, MSG_DEBUG, "%s", motd->data);
			if (!_strcmpi(motd->data, STR_STATUS) || !_strcmpi(motd->data, STR_INFO))
				MOTD_SendQueryData(motd, i);
		}
	}

	return;
}


DWORD WINAPI MOTD_MainThread(LPVOID param)
{
	motd_data_s			motd;
	int					ret;

	memset(&motd, 0, sizeof(motd));

	motd.server_data = (server_data_s *)param;

	for (ret = 0; ret < motd.server_data->cfg.msm.lines; ret++)
	{
		if (!MOTD_InitializeSocket(&motd.sock[ret], motd.server_data->cfg.msm.listen_port + ret))
		{
			Print(&motd.server_data->cfg, MSG_WARNING, 
				"MOTD: Failed to initialize socket: %d",
				WSAGetLastError());
			ExitThread(0);
		}
		else
		{
			Print(&motd.server_data->cfg, MSG_SYSTEM, "MOTD: Listening on port: %d",
				motd.server_data->cfg.msm.listen_port + ret);
		}
	}

	while (1)
	{
		MOTD_GetPacket(&motd);

		Sleep(motd.server_data->cfg.sleep_time);
	}

	ExitThread(0);
}


int MOTD_ReadFile(server_data_s *server_data)
{
	FILE			*f;
	char			c;
	int				i = 0, k = 0, m = 0;
	BOOL			act = FALSE;
	unsigned char	temp[128] = {0};


	if (server_data->cfg.msm.motd_file == NULL)
		return 0;

	f = fopen(server_data->cfg.msm.motd_file, "rb");
	if (f == NULL)
		return 0;

	while ((c = fgetc(f)) != EOF)
	{
		if (c == '\"')
		{
			act = act ? FALSE : TRUE;
			if (!act)
			{
				if (i == 0)
				{
					strncpy(server_data->cfg.msm.motd_lines_host[m], temp, 
						sizeof(server_data->cfg.msm.motd_lines_host[m]) - 1);
					i++;
				}
				else if (i == 1)
				{
					strncpy(server_data->cfg.msm.motd_lines_map[m], temp, 
						sizeof(server_data->cfg.msm.motd_lines_map[m]) - 1);
					i++;
				}
				else if (i == 2)
				{
					strncpy(server_data->cfg.msm.motd_lines_gametype[m], temp, 
						sizeof(server_data->cfg.msm.motd_lines_gametype[m]) - 1);
					i = 0;
					m++;
				}

				memset(temp, 0, sizeof(temp));
				k = 0;
			}

			continue;
		}

		else if (c == '\n' || c == '\r')
			continue;

		if (act)
		{
			if (k < sizeof(temp))
				temp[k] = c;
			k++;
		}
	}

	fclose(f);
	return m;
}


void MOTD_Print(server_data_s *server_data)
{
	int			i;

	Print(&server_data->cfg, MSG_NORMAL_EXT, "Printing %d MOTD lines:", 
		server_data->cfg.msm.lines);

	for (i = 0; i < server_data->cfg.msm.lines; i++)
		Print(&server_data->cfg, MSG_NORMAL, "%s %s %s", 
		server_data->cfg.msm.motd_lines_host[i],
		server_data->cfg.msm.motd_lines_map[i],
		server_data->cfg.msm.motd_lines_gametype[i]);

	return;
}


void MOTD_Start(server_data_s *server_data)
{
	HANDLE		tHandle;
	DWORD		id = 0;

	if ((server_data->cfg.msm.lines = MOTD_ReadFile(server_data)) == 0)
		return;

	MOTD_Print(server_data);

	tHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MOTD_MainThread, 
		(LPVOID)server_data, 0, &id);

	if (tHandle == NULL)
		Print(&server_data->cfg, MSG_WARNING, "Failed to create MOTD thread. Error: %d",
			GetLastError());
}