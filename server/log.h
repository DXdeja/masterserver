/*
	Unreal MasterServer Emulator v1.0
	Copyright (c) DejaVu
*/


typedef enum
{
	MSG_ERROR = 0,
	MSG_WARNING,
	MSG_SYSTEM,
	MSG_NORMAL,
	MSG_NORMAL_EXT,
	MSG_DEBUG
} print_msg_type;

void Print(config_s *cfg, print_msg_type type, char * format, ...);
void LOG_Initialize(config_s *cfg);