/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


#include <windows.h>
#include <stdio.h>

#include "defines.h"
#include "structs.h"
#include "log.h"


typedef struct _clients_data_s
{
	SOCKET				listen_sock;
	SOCKET				client_sock;
	struct sockaddr_in	from;
	server_data_s		*server_data;
	BOOL				gotinfo;
} clients_data_s;

typedef struct _client_s
{
	SOCKET				sock;
	struct sockaddr_in	address;
	server_data_s		*server_data;
	unsigned char		data[MAX_RECV_BUFFER];
} client_s;


int MSM_AddClient(client_s *cl)
{
	msm_clients_s		*msm = (msm_clients_s *)malloc(sizeof(msm_clients_s));

	if (msm == NULL)
	{
		Print(&cl->server_data->cfg, MSG_WARNING, 
			"MSM: Failed to allocate memory space!");
		return 0;
	}
	memset(msm, 0, sizeof(msm_clients_s));

	msm->prev = cl->server_data->msm;
	if (cl->server_data->msm != NULL)
		cl->server_data->msm->next = msm;
	cl->server_data->msm = msm;

	memcpy(&msm->address, &cl->address, sizeof(msm->address));
	msm->motd_shown = cl->server_data->current_time;

	Print(&cl->server_data->cfg, MSG_DEBUG, "MSM: New client added: %s",
		inet_ntoa(msm->address.sin_addr));

	return 1;
}


int MSM_CheckClient(client_s *cl)
{
	msm_clients_s		*msm;
	int					ret = 1;

	EnterCriticalSection(&cl->server_data->cs);
	msm = cl->server_data->msm;
	while (msm != NULL)
	{
		if (!memcmp(&cl->address.sin_addr, &msm->address.sin_addr, 
			sizeof(cl->address.sin_addr)))
		{
			ret = 0;
			break;
		}
		msm = msm->prev;
	}
	LeaveCriticalSection(&cl->server_data->cs);

	if (!ret)
		return 0;

	EnterCriticalSection(&cl->server_data->cs);
	ret = MSM_AddClient(cl);
	LeaveCriticalSection(&cl->server_data->cs);

	return ret;
}


void MSM_SendServers(client_s *cl)
{
	int		i;

	for (i = 0; i < cl->server_data->cfg.msm.lines; i++)
	{
		sprintf(cl->data, "\\ip\\%s:%d", 
			cl->server_data->cfg.msm.ip,
			cl->server_data->cfg.msm.listen_port + i);
		Print(&cl->server_data->cfg, MSG_DEBUG, "Sending: %s", cl->data);
		send(cl->sock, cl->data, (int)strlen(cl->data), 0);
	}

	send(cl->sock, STR_FINAL, STR_FINAL_LEN, 0);
}


SOCKET CL_SocketInitialize(clients_data_s *clients_data)
{
	int					on;

	Print(&clients_data->server_data->cfg, MSG_SYSTEM, "Opening TCP port: %d", 
		clients_data->server_data->cfg.tcp_port);

	// initialize new socket
	if ((clients_data->listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) 
		== INVALID_SOCKET) 
	{
		Print(&clients_data->server_data->cfg, MSG_ERROR, 
			"TCP socket initialization failed: %d", 
			WSAGetLastError());

		return 0;
	}

	if (setsockopt(clients_data->listen_sock, SOL_SOCKET, 
		SO_REUSEADDR, (char *) &on, sizeof(on)) == SOCKET_ERROR) 
	{
		Print(&clients_data->server_data->cfg, MSG_ERROR, 
			"Failed to perform setsockopt: %d", 
			WSAGetLastError());

		closesocket(clients_data->listen_sock);
		return 0;
	}

	clients_data->from.sin_addr.s_addr = INADDR_ANY;
	clients_data->from.sin_port = htons(clients_data->server_data->cfg.tcp_port);
	clients_data->from.sin_family = AF_INET;

	// bind socket to TCP port
	if (bind(clients_data->listen_sock, (struct sockaddr *)&clients_data->from, 
		sizeof(clients_data->from)) == SOCKET_ERROR) 
	{
		Print(&clients_data->server_data->cfg, MSG_ERROR, 
			"Failed to bind TCP socket: %d", 
			WSAGetLastError());

		closesocket(clients_data->listen_sock);
		return 0;
	}

	if (listen(clients_data->listen_sock, MAX_PENDING_CONNECTIONS) == SOCKET_ERROR)
	{
		Print(&clients_data->server_data->cfg, MSG_ERROR, 
			"TCP listen error: %d", 
			WSAGetLastError());

		closesocket(clients_data->listen_sock);
		return 0;
	}

	Print(&clients_data->server_data->cfg, MSG_SYSTEM, "TCP port %d binded", 
		clients_data->server_data->cfg.tcp_port);

	return 1;
}


BOOL CL_DropClient(clients_data_s *clients_data)
{
	FILE			*f;
	unsigned char	buff[32];
	size_t			i;

	if (clients_data->server_data->cfg.ban_file == NULL)
		return FALSE;

	f = fopen(clients_data->server_data->cfg.ban_file, "rb");
	if (f == NULL)
		return FALSE;

	while (fscanf(f, "%s", buff) > 0)
	{
		i = strlen(buff);
		if (!strncmp(inet_ntoa(clients_data->from.sin_addr), buff, i))
		{
			fclose(f);
			Print(&clients_data->server_data->cfg, MSG_DEBUG, 
				"Found match in ban list: %s (len: %d)",
				buff, i);
			Print(&clients_data->server_data->cfg, MSG_DEBUG, 
				"Dropping client %s:%d",
				inet_ntoa(clients_data->from.sin_addr),
				ntohs(clients_data->from.sin_port));

			return TRUE;
		}
	}

	fclose(f);
	return FALSE;
}


void CL_SendServers(client_s *cl)
{
	servers_s		*srv;

	srv = cl->server_data->servers;

    while (srv != NULL)
	{
		if (srv->show)
		{
			sprintf(cl->data, "\\ip\\%s:%d", 
				inet_ntoa(srv->address.sin_addr),
				srv->q_port);
			send(cl->sock, cl->data, (int)strlen(cl->data), 0);
		}
		srv = srv->prev;
	}

	send(cl->sock, STR_FINAL, STR_FINAL_LEN, 0);
}


int CL_Process(client_s *cl)
{
	struct timeval		tv;
	fd_set				fds;
	int					i, len = 0;
	unsigned char		*p, *q;

	tv.tv_sec = cl->server_data->cfg.client_timeout;
	tv.tv_usec = 0;

	// first packet to send (note: last byte is null)
	if (send(cl->sock, STR_BASICSECURE, STR_BASICSECURE_LEN, 0) <= 0)
		return 0;

	// we need to read byte per byte until \final\ received
	while (strstr(cl->data, "\\final\\") == NULL)
	{
		FD_ZERO(&fds); 
		FD_SET(cl->sock, &fds);

		if (len == MAX_RECV_BUFFER)
			break;

		if (select(0, &fds, NULL, NULL, &tv) > 0)
		{
			i = recv(cl->sock, cl->data + len, 1, 0);
			if (i <= 0)
				return 0;
			else
				len += i;
		}
		else
			return 0;
	}

	if (cl->data[0] != '\\')
		return 0;

	p = cl->data + 1;
	for (q = p; *q != '\\' && ((q - p) < (STR_GAMENAME_LEN + 1)); q++) 
	{
		if (!*q)
			return 0;
	}
	*q = 0;
	if (_strcmpi(p, STR_GAMENAME))
		return 0;
	q++;
	for (p = q; *q != '\\' && ((q - p) < ((int)strlen(cl->server_data->cfg.game_name) + 1)); q++)
	{
		if (!*q)
			return 0;
	}
	*q = 0;
	if (_strcmpi(p, cl->server_data->cfg.game_name))
		return 0;

	// if theres any data waiting, we need to receive it
	// else our send data won't be sent
	tv.tv_sec = 0;
	FD_ZERO(&fds); 
	FD_SET(cl->sock, &fds);
	if (select(0, &fds, NULL, NULL, &tv) > 0)
		recv(cl->sock, cl->data, sizeof(cl->data) - 1, 0);

	if (cl->server_data->cfg.msm.motd_file != NULL)
	{
		if (MSM_CheckClient(cl))
		{
			Print(&cl->server_data->cfg, MSG_NORMAL, "Client getting motd: %s:%d",
				inet_ntoa(cl->address.sin_addr),
				ntohs(cl->address.sin_port));

			MSM_SendServers(cl);

			return 1;
		}
	}

	Print(&cl->server_data->cfg, MSG_NORMAL, "Client getting list: %s:%d",
		inet_ntoa(cl->address.sin_addr),
		ntohs(cl->address.sin_port));

	CL_SendServers(cl);

	return 1;
}


DWORD WINAPI CL_Thread(LPVOID param)
{
	client_s			cl;
	clients_data_s		*clients_data = (clients_data_s *)param;

	memset(&cl, 0, sizeof(cl));
	cl.server_data = clients_data->server_data;
	cl.address.sin_addr = clients_data->from.sin_addr;
	cl.address.sin_port = clients_data->from.sin_port;
	cl.sock = clients_data->client_sock;
	clients_data->gotinfo = TRUE;

	Print(&cl.server_data->cfg, MSG_DEBUG, "Started new client thread");
	Print(&cl.server_data->cfg, MSG_DEBUG, "Client: %s:%d, socket: %d",
		inet_ntoa(cl.address.sin_addr),
		ntohs(cl.address.sin_port),
		cl.sock);

	if (!CL_Process(&cl))
		Print(&cl.server_data->cfg, MSG_DEBUG, "Client error: %s:%d",
			inet_ntoa(cl.address.sin_addr),
			ntohs(cl.address.sin_port));
	
	Print(&cl.server_data->cfg, MSG_DEBUG, "Cleaning up for client: %s:%d",
		inet_ntoa(cl.address.sin_addr),
		ntohs(cl.address.sin_port));

	closesocket(cl.sock);
	ExitThread(0);
}


DWORD WINAPI CL_MainThread(LPVOID param)
{
	DWORD			id = 0;
	clients_data_s	clients_data;
	int				fsize;

	memset(&clients_data, 0, sizeof(clients_data));
	clients_data.server_data = (server_data_s *)param;

	Print(&clients_data.server_data->cfg, MSG_DEBUG, "Client thread started.");
	
	if (!CL_SocketInitialize(&clients_data))
	{
		Print(&clients_data.server_data->cfg, MSG_ERROR, "Exiting client thread.");
		Print(&clients_data.server_data->cfg, MSG_ERROR, 
			"No clients will be able to get server list!");
		ExitThread(0);
	}

	fsize = sizeof(clients_data.from);

	while (1) 
	{
		clients_data.client_sock = SOCKET_ERROR;

		while (clients_data.client_sock == SOCKET_ERROR ) 
		{
			clients_data.client_sock = accept(clients_data.listen_sock, 
				(struct sockaddr *)&clients_data.from, &fsize);
		}

		if (clients_data.client_sock != INVALID_SOCKET) 
		{
			Print(&clients_data.server_data->cfg, MSG_DEBUG, 
				"New client connecting: %s:%d, socket: %d",
				inet_ntoa(clients_data.from.sin_addr),
				ntohs(clients_data.from.sin_port),
				clients_data.client_sock);
			if (!CL_DropClient(&clients_data))
			{
				clients_data.gotinfo = FALSE;
				if (CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CL_Thread, 
					(LPVOID)&clients_data, 0, &id) == NULL)
					Print(&clients_data.server_data->cfg, MSG_DEBUG, 
						"Failed to create new client thread: %d",
						GetLastError());
				else
					while (!clients_data.gotinfo)
						Sleep(1);	
			}
		}
	}

	ExitThread(0);
}


BOOL CL_Start(server_data_s *server_data)
{
	HANDLE		tHandle;
	DWORD		id = 0;

	tHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CL_MainThread, 
		(LPVOID)server_data, 0, &id);

	if (tHandle == NULL)
	{
		Print(&server_data->cfg, MSG_ERROR, "Failed to create client thread. Error: %d",
			GetLastError());
		Print(&server_data->cfg, MSG_ERROR, "No clients will be able to get server list!");
		return FALSE;
	}
	else
		return TRUE;
}