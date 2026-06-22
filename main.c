#define VERSION			"HPAK34401ACalTool Ver 1.0 2026-06-22"
#define SIGNATURE		"(C)2026 squad"
#define HELP_STRINGS  	("\
USAGE: HPAK34401ACalTool.exe [mode] [-cp|--com-port [COM|com<decimal>]] [-fp|--file-path \"<path>\"]\n\n\
mode:\n\
 -h, --help\t\tdisplay this help and exit.\n\
 -dm, --dump-mode\tDump Calibration from 34401A to computer.\n\
 \t\t\tIf the --com-port option is not specified, the default output path is used.\n\
 \t\t\t(\"currentDirectory\\caldump.bin\")\n\
 -fm, --flash-mode\tFlashing Calibration from computer to 34401A. (not implemented)\n\n\
Example:\n\
HPAK34401ACalTool --dump-mode --com-port COM3\n\
HPAK34401ACalTool -dm -cp COM3 -fp \"<filepath>\"\n\
HPAK34401ACalTool -fm -cp COM3 -fp \"<filepath>\"\n\n")

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "libhp34401A.h"

int dumpCal(char *dumpPath, char *port);
int flashCal(char *dumpPath, char *port);

int open_comport(char *portname, int baudrate, int bits, enum sp_parity parity, int stopbits, enum sp_flowcontrol flowcontrol, struct sp_port **port);

int main(int argc, char **argv)
{
	printf("%s %s", VERSION, SIGNATURE);
	printf("\r\n\r\n");

	int mode = 0;
	int ret = -1;

	if(argc < 2){
		printf(HELP_STRINGS);
		printf("[ERROR]missing arguments.\n");
		goto err1;
	}

	char  *current_directory = NULL;
	if(!(current_directory = getcwd(current_directory, 0))){
		printf("[ERROR]getcwd error, errno = %d, strerror = %s\n", errno, strerror(errno));
		goto err1;
	}

	char *port = NULL;
	char *file_path = NULL;

	for(int i = 1; i < argc; i++){
		if((strstr(argv[i], "-H") - argv[i]) == 0 || (strstr(argv[i], "-h") - argv[i]) == 0 || (strstr(argv[i], "--help") - argv[i]) == 0){
			ret = 0;
			goto err1;
		}

		if((strstr(argv[i], "-DM") - argv[i]) == 0 || (strstr(argv[i], "-dm") - argv[i]) == 0 || (strstr(argv[i], "--dump-mode") - argv[i]) == 0){
			mode = 1;
		}

		if((strstr(argv[i], "-FM") - argv[i]) == 0 || (strstr(argv[i], "-fm") - argv[i]) == 0 || (strstr(argv[i], "--flash-mode") - argv[i]) == 0){
			mode = 2;
		}

		if((strstr(argv[i], "-CP") - argv[i]) == 0 || (strstr(argv[i], "-cp") - argv[i]) == 0 || (strstr(argv[i], "--com-port") - argv[i]) == 0){
			if(argv[i + 1] != NULL){
				port = argv[i + 1];
			}
		}

		if((strstr(argv[i], "-FP") - argv[i]) == 0 || (strstr(argv[i], "-fp") - argv[i]) == 0 || (strstr(argv[i], "--file-path") - argv[i]) == 0){
			if(argv[i + 1] != NULL){
				file_path = argv[i + 1];
			}
		}
	}

	if(port == NULL){
		printf("[ERROR]COM port is not specified.\n");
		goto err2;
	}

	printf("[INFO]COM port = %s\n", port);

	switch(mode){
		case 1:

			int filepath_alloc_flag = 0;
			if(file_path == NULL){
				if((file_path = calloc(strlen(current_directory) + 1 + strlen("caldump.bin\0") + 1, 1)) == NULL){
					printf("[ERROR]calloc error.\n");
					goto err2;
				}

				memcpy(file_path, current_directory, strlen(current_directory));
				file_path[strlen(file_path)] = '\\';
				strcat(file_path, "caldump.bin");

				printf("[WARNING]Output Calibration file path is not specified. using default path.\n");
				filepath_alloc_flag = 1;
			}
			printf("[INFO]Output Calibration file path = %s\n", file_path);

			printf("[INFO]Dump Calibration mode.\n");
			if(dumpCal(file_path, port) < 0){
				if(filepath_alloc_flag) free(file_path);
				goto err2;
			}

			if(filepath_alloc_flag) free(file_path);
			break;

		case 2:

			if(file_path == NULL){
				printf("[ERROR]Input Calibration file path is not specified.\n");
				goto err2;
			}
			printf("[INFO]Input Calibration file path = %s\n", file_path);

			printf("[INFO]Flash Calibration mode.\n");
			if((ret = flashCal(file_path, port)) < 0){
				goto err2;
			}
			break;

		default:
			printf("[ERROR]Unknown mode.\n");
			goto err2;
	}

	ret = 0;
err2:
	free(current_directory);
err1:
	printf("[INFO]finished.\n");
	printf("press enter key to exit...");
	getchar();
	return ret;
}

int dumpCal(char *dumpPath, char *port)
{
	int ret = -1;

	struct sp_port *portHandle;

	if(open_comport(port, 9600, 8, SP_PARITY_NONE, 2, SP_FLOWCONTROL_DTRDSR, &portHandle)) goto err1;

	if(sp_flush(portHandle, SP_BUF_BOTH)){
		printf("[ERROR]sp_flush error.\n");
		goto err2;
	}

	int rett = 0;
	if((rett = is34401A(portHandle)) < 1){

		if(rett == 0){
			printf("[ERROR]Target is not HPAK 34401A.\n");
			goto err2;
		}

		printf("[ERROR]is34401A error.\n");
		goto err2;
	}

	FILE *filehandle;

	if((filehandle = fopen(dumpPath, "wb")) == NULL){
		printf("[ERROR]dump file create error, errno = %d, strerror = %s\n", errno, strerror(errno));
		goto err2;
	}

	hp34401ASendCommand(portHandle, "SYST:RWL");

	hp34401APrintText(portHandle, 0, "CALDUMP STRT");
	hp34401ASendCommand(portHandle, "SYST:BEEP;");
	hp34401ASendCommand(portHandle, "SYST:BEEP;");
	sleep(2);

	for(int memCnt = 0; memCnt < 256; memCnt++){
		printf("[INFO]dumped:%3d%%\r", (int)(((float)memCnt / 255) * 100));
		hp34401APrintText(portHandle, 0, "CALDUMP %3d%%", (int)(((float)memCnt / 255) * 100));
		char *readBuf = NULL;

		char sendCommandBuf[30];
		sprintf(sendCommandBuf, "DIAG:PEEK? -1,%d,0", memCnt);

		if(hp34401ASendCommandWithRead(portHandle, sendCommandBuf, 0, &readBuf) < 0){
			printf("\n[ERROR]sendCommandWithRead error.\n");
			goto err3;
		}

		unsigned int readData = strtol(readBuf, NULL, 10);

		if(fwrite(&readData, 1, 2, filehandle) < 2){
			printf("\n[ERROR]fwrite error.\n");
		}

		free(readBuf);
	}

	printf("\n");
	printf("[INFO]finished dump.\n");

	hp34401APrintText(portHandle, 0, "CALDUMP END");
	hp34401ASendCommand(portHandle, "SYST:BEEP;");
	hp34401ASendCommand(portHandle, "SYST:BEEP;");
	sleep(2);

	hp34401ASendCommand(portHandle, "SYST:BEEP;");
	hp34401ASendCommand(portHandle, "SYST:LOC");

	ret = 0;
err3:
	fclose(filehandle);

err2:
	if(sp_close(portHandle) < 0) { printf("[ERROR]COM port close error.\n"); ret = -1; goto err1; }
	sp_free_port(portHandle);
	printf("[INFO]COM port = %s, closed.\n", port);
err1:
	return ret;
}

int flashCal(char *dumpPath, char *port)
{
	return 0;
}

//functions

int open_comport(char *portname, int baudrate, int bits, enum sp_parity parity, int stopbits, enum sp_flowcontrol flowcontrol, struct sp_port **port)
{
	if(sp_get_port_by_name(portname, port) < 0){
		printf("[ERROR]COM port = %s, error\n", portname);
		return -1;
	}

	if(sp_open(*port, SP_MODE_READ_WRITE) < 0){
		printf("[ERROR]COM port = %s, open error\n", portname);
		return -2;
	}
	if(sp_set_baudrate(*port, baudrate) < 0){
		printf("[ERROR]failed to set baudrate.\n");
		return -3;
	}
	if(sp_set_bits(*port, bits) < 0){
		printf("[ERROR]failed to set bits.\n");
		return -4;
	}
	if(sp_set_parity(*port, parity) < 0){
		printf("[ERROR]failed to set parity.\n");
		return -5;
	}
	if(sp_set_stopbits(*port, stopbits) < 0){
		printf("[ERROR]failed to set stopbits.\n");
		return -6;
	}
	if(sp_set_flowcontrol(*port, flowcontrol) < 0){
		printf("[ERROR]failed to set flowcontrol setting.\n");
		return -7;
	}

	if(sp_flush(*port, SP_BUF_BOTH) < 0){
		printf("[ERROR]failed to sp_flush.\n");
		return -8;
	}

	printf("[INFO]COM port = %s, opened.\n", portname);
	return 0;
}
