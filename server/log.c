/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "defines.h"
#include "structs.h"
#include "log.h"

#define MSG_PREFIX_SIZE		64
#define MSG_LOG_LEN			2048


typedef struct _print_msg_s
{
	print_msg_type	type;
	unsigned char	*text;
	unsigned short	color;
} print_msg_s;

print_msg_s	print_msg[] = {
	{MSG_ERROR, " - ERROR - ", 0x004F},
	{MSG_WARNING, " - WARNING - ", 0x006F},
	{MSG_SYSTEM, " - SYSTEM - ", 0x000F},
	{MSG_NORMAL, "", 0x0007},
	{MSG_NORMAL_EXT, "", 0x000B},
	{MSG_DEBUG, " - DEBUG - ", 0x0006},
};

void SetColor(unsigned short color)
{                                                  
    HANDLE hcon = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(hcon, color);
}

void Print(config_s *cfg, print_msg_type type, char * format, ...)
{
	unsigned char	buf[MSG_PREFIX_SIZE];
	unsigned char	buffer[MSG_LOG_LEN];
	SYSTEMTIME		td;
	va_list			args;
	FILE			*f = NULL;

	if (!cfg->debug_mode && type == MSG_DEBUG)
		return;

	if (cfg->log_file != NULL)
		f = fopen(cfg->log_file, "ab");

	GetLocalTime(&td);

	SetColor(print_msg[type].color);

	sprintf(buf, "(%02d:%02d:%02d)%s: ", td.wHour, td.wMinute, td.wSecond, print_msg[type].text);
	printf("%s", buf);
	if (f != NULL)
		fputs(buf, f);

	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	printf("%s\n", buffer);

	if (f != NULL)
	{
		fputs(buffer, f);
		fputs("\n", f);
		fclose(f);
	}
	
	return;
}

void LOG_Initialize(config_s *cfg)
{
	FILE			*f;
	SYSTEMTIME		td;
	unsigned char	buf[MSG_PREFIX_SIZE];

	if (cfg->log_file != NULL)
		f = fopen(cfg->log_file, "ab");
	else
		f = NULL;

	if (f != NULL)
	{
		GetLocalTime(&td);
		fputs("-= LOG FILE STARTED =-\n", f);
		sprintf(buf, "Date: %02d-%02d-%04d\n", td.wDay, td.wMonth, td.wYear);
		fputs(buf, f);
		sprintf(buf, "Time: %02d:%02d:%02d\n", td.wHour, td.wMinute, td.wSecond);
		fputs(buf, f);
		fputs("-= LOG FILE STARTED =-\n", f);
		fclose(f);
		Print(cfg, MSG_SYSTEM, "Log file opened: %s", cfg->log_file);
	}
	else
		Print(cfg, MSG_WARNING, "Log file failed to open: %s", cfg->log_file); 

	return;
}