/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


#include <windows.h>
#include <stdio.h>

#include "defines.h"
#include "structs.h"
#include "log.h"


int SRV_SocketInitialize(server_data_s *server_data)
{
	struct sockaddr_in	address;
	u_long				mode = 1;

	Print(&server_data->cfg, MSG_SYSTEM, "Opening UDP port: %d", server_data->cfg.udp_port);

	// initialize new socket
	if ((server_data->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) 
	{
		Print(&server_data->cfg, MSG_ERROR, "UDP socket initialization failed: %d", WSAGetLastError());
		return 0;
	}

	// make it non-blocking
	if (ioctlsocket(server_data->sock, FIONBIO, &mode) == SOCKET_ERROR ) 
	{
		Print(&server_data->cfg, MSG_ERROR, "Failed to set UDP socket to non blocking: %d", WSAGetLastError());
		closesocket(server_data->sock);
		return 0;
	}

	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(server_data->cfg.udp_port);
	address.sin_family = AF_INET;

	// bind socket to UDP port
	if (bind(server_data->sock, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) 
	{
		Print(&server_data->cfg, MSG_ERROR, "Failed to bind UDP socket: %d", WSAGetLastError());
		closesocket(server_data->sock);
		return 0;
	}

	Print(&server_data->cfg, MSG_SYSTEM, "UDP port %d binded", server_data->cfg.udp_port);

	return 1;
}

BOOL SRV_DropPacket(server_data_s *server_data)
{
	FILE			*f;
	unsigned char	buff[32];
	size_t			i;

	if (server_data->cfg.exc_file == NULL)
		return FALSE;

	f = fopen(server_data->cfg.exc_file, "rb");
	if (f == NULL)
		return FALSE;

	while (fscanf(f, "%s", buff) > 0)
	{
		i = strlen(buff);
		if (!strncmp(inet_ntoa(server_data->from.sin_addr), buff, i))
		{
			fclose(f);
			Print(&server_data->cfg, MSG_DEBUG, "Found match in exclude list: %s (len: %d)",
				buff, i);
			Print(&server_data->cfg, MSG_DEBUG, "Dropping msg (len=%d) from %s:%d",
				server_data->len,
				inet_ntoa(server_data->from.sin_addr),
				ntohs(server_data->from.sin_port));
			return TRUE;
		}
	}

	fclose(f);
	return FALSE;
}

void SRV_SendStatusPacket(server_data_s *server_data)
{
	Print(&server_data->cfg, MSG_DEBUG, "Sending status packet to: %s:%d",
		inet_ntoa(server_data->from.sin_addr),
		ntohs(server_data->from.sin_port));

	if (sendto(server_data->sock, 
		STR_STATUS, 
		STR_STATUS_LEN, 
		0, 
		(struct sockaddr *)&server_data->from, sizeof(server_data->from)) <= 0)
	{
		Print(&server_data->cfg, MSG_DEBUG, "Failed sending status packet to: %s:%d",
			inet_ntoa(server_data->from.sin_addr),
			ntohs(server_data->from.sin_port));
	}
}


int SRV_GetPacket(server_data_s *server_data)
{
	int		fromlen = sizeof(server_data->from);

	memset(&server_data->data, 0, sizeof(server_data->data));
	server_data->len = recvfrom(server_data->sock, 
		server_data->data, 
		sizeof(server_data->data) - 1, 
		0,
		(struct sockaddr *)&server_data->from,
		&fromlen);

	if (server_data->len == SOCKET_ERROR)
	{
		server_data->len = WSAGetLastError();

		if (server_data->len != WSAEWOULDBLOCK && server_data->len != WSAECONNRESET)
			Print(&server_data->cfg, MSG_WARNING, "Recv error: %d", server_data->len);

		return 0;
	}

	if (!SRV_DropPacket(server_data))
	{
		Print(&server_data->cfg, MSG_DEBUG, "Received msg (len=%d) from %s:%d",
			server_data->len,
			inet_ntoa(server_data->from.sin_addr),
			ntohs(server_data->from.sin_port));
		Print(&server_data->cfg, MSG_DEBUG, "%s", server_data->data);

		return 1;
	}

	return 0;
}

void SRV_AddToList(server_data_s *server_data)
{
	servers_s	*n = (servers_s *)malloc(sizeof(servers_s));

	if (n == NULL)
	{
		Print(&server_data->cfg, MSG_WARNING, 
			"Failed to allocate memory space for new server");
		return;
	}

	memset(n, 0, sizeof(servers_s));

	n->prev = server_data->servers;
	if (server_data->servers != NULL)
		server_data->servers->next = n;
	server_data->servers = n;

	memcpy(&n->address, &server_data->from, sizeof(n->address));
	n->hb_time = server_data->current_time;
	n->q_port = server_data->q_port;

	Print(&server_data->cfg, MSG_DEBUG, "New server added: %s:%d",
		inet_ntoa(n->address.sin_addr),
		n->q_port);

	return;
}

void SRV_AddNew(server_data_s *server_data) 
{
	servers_s	*srv;

	// check if server is already in list
	srv = server_data->servers;
	while (srv != NULL)
	{
		if (!memcmp(&server_data->from.sin_addr, 
			&srv->address.sin_addr, 
			sizeof(server_data->from.sin_addr)) &&
			server_data->q_port == srv->q_port)
		{
			// server is already in list
			// update hb time only
			// set state to HB_RECV, but do not set show to FALSE
			// (in case of spoofed attack, someone could
			// remove servers that way)
			srv->hb_time = server_data->current_time;
			srv->state = HB_RECV;

			Print(&server_data->cfg, MSG_DEBUG, "Server rejoining: %s:%d",
				inet_ntoa(srv->address.sin_addr),
				srv->q_port);

			return;
		}

		srv = srv->prev;
	}

	// if not, add it
	SRV_AddToList(server_data);

	return;
}

int SRV_Approve(server_data_s *server_data) 
{
	servers_s	*srv;

	// lets find our server first
	srv = server_data->servers;
    while (srv != NULL) 
	{
		if (!memcmp(&server_data->from.sin_addr, 
			&srv->address.sin_addr, 
			sizeof(server_data->from.sin_addr)) &&
			ntohs(server_data->from.sin_port) == srv->q_port)
		{
			if (srv->state == DENIED)
				return 0;

			srv->last_access = server_data->current_time;
			srv->state = APPROVED;
			srv->ssent = 0;
			if (!srv->show)
			{
				srv->show = TRUE;
				Print(&server_data->cfg, MSG_NORMAL_EXT, "New server: %s:%d",
					inet_ntoa(srv->address.sin_addr),
					srv->q_port);
			}

			return 1;
        }
		srv = srv->prev;
    }

	return 0;
}


servers_s *SRV_Remove(servers_s *srv, server_data_s *server_data)
{
	servers_s	*n;

	n = srv;
	srv = srv->prev;

	if (n->next != NULL)
		n->next->prev = srv;
	// check if removing server is on top (also adjust server_data->servers pointer)
	else
		server_data->servers = srv;
	if (n->prev != NULL)
		n->prev->next = n->next;

	free(n);

	return srv;
}

void SRV_Refresh(server_data_s *server_data)
{
	DWORD		srv_time, status_time;
	servers_s	*srv;

	srv_time = server_data->current_time - server_data->cfg.server_timeout;
	status_time = server_data->current_time - server_data->cfg.server_status_timeout;

	srv = server_data->servers;
	while (srv != NULL)
	{
		// server is probably offline as there has not been any new heartbeats from it
        if (srv->state == APPROVED && srv->last_access < srv_time) 
		{
			Print(&server_data->cfg, MSG_NORMAL_EXT, "Server gone: %s:%d",
				inet_ntoa(srv->address.sin_addr),
				srv->q_port);

            srv = SRV_Remove(srv, server_data);
			continue;
        }
		// server hasnt sent status packet yet and there were many status packets sent
		// already, fake server?
		else if (srv->state == S_SENT && (srv->hb_time < status_time ||
			srv->ssent >= server_data->cfg.max_resend_packets))
		{
			Print(&server_data->cfg, MSG_DEBUG, "Server failed: %s:%d",
				inet_ntoa(srv->address.sin_addr),
				srv->q_port);

			srv = SRV_Remove(srv, server_data);
			continue;
		}
		// server hasn't sent status packet back to us yet
		// if time for resend passed, send another packet
		else if (srv->state == S_SENT && 
			(srv->ss_time + server_data->cfg.resend_delay) < 
			server_data->current_time)
		{
			server_data->from.sin_family = AF_INET;
			server_data->from.sin_addr = srv->address.sin_addr;
			server_data->from.sin_port = htons(srv->q_port);
			SRV_SendStatusPacket(server_data);
			srv->ss_time = server_data->current_time;
			srv->ssent++;	
		}
		// we haven't sent status packet yet
		else if (srv->state == HB_RECV)
		{
			server_data->from.sin_family = AF_INET;
			server_data->from.sin_addr = srv->address.sin_addr;
			server_data->from.sin_port = htons(srv->q_port);
			SRV_SendStatusPacket(server_data);
			srv->ss_time = server_data->current_time;
			srv->ssent++;
			srv->state = S_SENT;
		}

		srv = srv->prev;
	}
}

void SRV_Count(server_data_s *server_data) 
{
	servers_s		*srv;
	unsigned char	buff[256];
	int				pending = 0, 
					visible = 0;

	srv = server_data->servers;

    while (srv != NULL) 
	{
        if ((srv->state == HB_RECV || 
			srv->state == S_SENT || srv->state == APPROVED) 
			&& srv->show == FALSE)
			pending++;
		else if (srv->show == TRUE)
			visible++;

		srv = srv->prev;
    }

	sprintf(buff, CONSOLE_TITLE, visible, pending);
	SetConsoleTitleA(buff);

	return;
}


int SRV_ParsePacket(server_data_s *server_data)
{
	unsigned char	*p, *q;

	// drop the packet if it doesn't start with "\"
	if (server_data->data[0] != '\\')
		return 0;

	p = server_data->data + 1;
	if (!strncmp(p, STR_HEARTBEAT, STR_HEARTBEAT_LEN))
	{
		p += STR_HEARTBEAT_LEN;
		*p = 0;
		p++;
		for (q = p; *q != '\\' && ((q - p) < 8); q++) 
		{
			if (!*q)
				return 0;
		}
		*q = 0;
		server_data->q_port = atoi(p);
		if (server_data->q_port == 0)
			return 0;
		Print(&server_data->cfg, MSG_DEBUG, "Server port: %d", server_data->q_port);
		q++;
		for (p = q; *p != '\\' && ((p - q) < (STR_GAMENAME_LEN + 1)); p++) 
		{
			if (!*q)
				return 0;
		}
		*p = 0;
		if (strcmp(q, STR_GAMENAME))
			return 0;
		p++;
		if (_strcmpi(p, server_data->cfg.game_name))
			return 0;

		Print(&server_data->cfg, MSG_DEBUG, "Server gamename: %s", p);

		SRV_AddNew(server_data);

		return 1;
	}
	else
	{
		// else assume it is status packet (we need game port, 
		// then we approve the server)
		while (*p)
		{
			for (q = p; *q != '\\' && ((q - p) < (STR_HOSTPORT_LEN + 1)); q++) 
			{
				if (!*q)
					return 0;
			}
			*q = 0;
			if (!_strcmpi(p, STR_HOSTPORT))
			{
				SRV_Approve(server_data);
				return 1;
			}
			p = q + 1;
		}

		return 0;
	}
}


void SRV_AddIncludes(server_data_s *server_data)
{
	FILE			*f;
	unsigned char	buff[32], *p;

	if (server_data->cfg.inc_file == NULL)
		return;

	f = fopen(server_data->cfg.inc_file, "rb");
	if (f == NULL)
		return;

	if ((server_data->current_time - server_data->include_time) > INCLUDE_REJOIN_TIME)
		server_data->include_time = server_data->current_time;
	else
	{
		fclose(f);
		return;
	}

	while (fscanf(f, "%s", buff) > 0)
	{
		for (p = buff; *p != ':'; p++) 
		{
			if (!*p)
				break;
		}
		if (!*p)
			continue;
		*p = 0;
		p++;
		server_data->q_port = atoi(p);
		if (server_data->q_port == 0)
			continue;

		server_data->from.sin_addr.s_addr = inet_addr(buff);
		server_data->from.sin_port = htons(server_data->q_port);

		Print(&server_data->cfg, MSG_DEBUG, "Adding include server: %s:%d",
			inet_ntoa(server_data->from.sin_addr),
			server_data->q_port);

		SRV_AddNew(server_data);
		SRV_Approve(server_data);
	}

	fclose(f);
	return;
}


msm_clients_s *MSM_Remove(msm_clients_s *msm, server_data_s *server_data)
{
	msm_clients_s	*n;

	n = msm;
	msm = msm->prev;

	if (n->next != NULL)
		n->next->prev = msm;
	// check if removing msm client is on top 
	// (also adjust server_data->msm pointer)
	else
		server_data->msm = msm;
	if (n->prev != NULL)
		n->prev->next = n->next;

	free(n);

	return msm;
}



void MSM_CheckClients(server_data_s *server_data)
{
	msm_clients_s		*msm;

	EnterCriticalSection(&server_data->cs);
	msm = server_data->msm;
	while (msm != NULL)
	{
		if ((server_data->current_time - msm->motd_shown) > 
			server_data->cfg.msm.motd_interval)
		{
			Print(&server_data->cfg, MSG_DEBUG, "MSM: Removing client %s",
				inet_ntoa(msm->address.sin_addr));
			msm = MSM_Remove(msm, server_data);
			continue;
		}
		msm = msm->prev;
	}
	LeaveCriticalSection(&server_data->cs);

	return;
}


void SRV_Main(server_data_s *server_data)
{
	if (!SRV_SocketInitialize(server_data))
		return;

	while (1)
	{
		server_data->current_time = GetTickCount();

		SRV_AddIncludes(server_data);

		if (SRV_GetPacket(server_data))
			SRV_ParsePacket(server_data);

		SRV_Refresh(server_data);

		SRV_Count(server_data);

		if (server_data->cfg.msm.motd_file != NULL)
			MSM_CheckClients(server_data);

		// sleep a bit
		Sleep(server_data->cfg.sleep_time);
	}
}