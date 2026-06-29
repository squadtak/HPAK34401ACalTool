#define VERSION			"HPAK34401ACalTool Ver 1.5 2026-06-29"
#define SIGNATURE		"(C)2026 squad"
#define HELP_STRINGS  	("\
USAGE: HPAK34401ACalTool [mode] [-cp|--com-port [COM|com<decimal>]] [-fp|--file-path \"<path>\"]\n\n\
mode:\n\
 -h, --help\t\tdisplay this help and exit.\n\
 -dm, --dump-mode\tDump Calibration from 34401A to computer.\n\
 \t\t\tIf the --file-path option is not specified, the default output path is used.\n\
 \t\t\t(\"currentDirectory\\caldump_xx-xx-xx.bin\")\n\
 -fm, --flash-mode\tFlashing Calibration from computer to 34401A. (not implemented)\n\n\
If no mode is specified, the default is dump mode.\n\
If no port is specified, it searches the 34401A among the available ports.\n\n\
Example:\n\
HPAK34401ACalTool\n\
HPAK34401ACalTool --dump-mode --com-port COM3\n\
HPAK34401ACalTool -dm -cp COM3 -fp \"<filepath>\"\n\
HPAK34401ACalTool -fm -cp COM3 -fp \"<filepath>\"\n\n")

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libserialport.h>
#include "libfile.h"
#include "libhp34401A.h"

int dumpCal(char **dumpPath, int undefPathFlag, char *port);
int flashCal(char *dumpPath, char *port);

int openComport(char *portName, int baud, int bits, enum sp_parity parity, int sbits, enum sp_flowcontrol fcontrol, struct sp_port **port);

int sendCommand(struct sp_port *port, char *command);
int sendCommandWithRead(struct sp_port *port, char *command, size_t delay_read_data, char **readData);
int printText(struct sp_port *port, size_t scrollDelay, char *format, ...);

int main(int argc, char **argv)
{
	printf("%s %s\n\n", VERSION, SIGNATURE);

	int ret = -1;

	int mode = 0;
	char *port = NULL;
	char *filePath = NULL;

	for(int i = 1; i < argc; i++){
		if((strstr(argv[i], "-H") - argv[i]) == 0 || (strstr(argv[i], "-h") - argv[i]) == 0 || (strstr(argv[i], "--help") - argv[i]) == 0){
			printf(HELP_STRINGS);
			ret = 0;
			goto err1;
		}

		if(!strcmp(argv[i], "-DM") || !strcmp(argv[i], "-dm") || !strcmp(argv[i], "--dump-mode")) mode = 1;

		if(!strcmp(argv[i], "-FM") || !strcmp(argv[i], "-fm") || !strcmp(argv[i], "--flash-mode")) mode = 2;

		if(!strcmp(argv[i], "-CP") || !strcmp(argv[i], "-cp") || !strcmp(argv[i], "--com-port")){
			if(argv[i + 1]) port = argv[i + 1];
		}

		if(!strcmp(argv[i], "-FP") || !strcmp(argv[i], "-fp") || !strcmp(argv[i], "--file-path")){
			if(argv[i + 1]) filePath = argv[i + 1];
		}
	}

	if(!mode) printf("[WARINIG]mode is not specified.\n");

	if(port == NULL){
		printf("[WARINIG]COM port is not specified.\n");
	}else{
		printf("[INFO]COM port = %s\n", port);
	}


	switch(mode){

		default:
		case 1:{

			int undefPathFlag = 0;
			if(!filePath){
				printf("[WARNING]Output Calibration file path is not specified. using default path.\n");

				if(!(filePath = getcwd(filePath, 0))){
					printf("[ERROR]getcwd error.\n");
					goto err1;
				}

				undefPathFlag = 1;
			}

			printf("[INFO]Dump Calibration mode.\n");
			if(dumpCal(&filePath, undefPathFlag, port)){
				if(undefPathFlag) free(filePath);
				goto err1;
			}

			if(undefPathFlag) free(filePath);

			break;
		}

		case 2:{

			if(filePath == NULL){
				printf("[ERROR]Input Calibration file path is not specified.\n");
				goto err1;
			}

			printf("[INFO]Input Calibration file path = %s\n", filePath);

			printf("[INFO]Flash Calibration mode.\n");
			if((ret = flashCal(filePath, port)) < 0){
				goto err1;
			}

			break;
		}
	}

	ret = 0;
err1:
	printf("[INFO]finished.\n");
	printf("press enter key to exit...");
	getchar();
	return ret;
}

int dumpCal(char **dumpPath, int undefPathFlag, char *port)
{
	int ret = -1;

	struct sp_port *portHandle = NULL;
	struct sp_port **portList = NULL;
	if(!port){

		if(sp_list_ports(&portList)){
			printf("[ERROR]sp_list_ports error.\n");
			goto err1;
		}

		for(int i = 0; portList[i]; i++){

			portHandle = portList[i];
			port = sp_get_port_name(portList[i]);

			if(openComport(port, 9600, 8, SP_PARITY_NONE, 2, SP_FLOWCONTROL_DTRDSR, &portHandle)) continue;

			if(is34401A(portHandle) == 1){
				printf("[INFO]found HPAK 34401A port.\n");
				goto foundPort;
			}

			if(sp_close(portHandle) < 0) { printf("[ERROR]COM port close error.\n"); goto err1; }
			printf("[INFO]COM port = %s, closed.\n", port);
		}

		printf("[ERROR]not found HPAK 34401A port.\n");
		sp_free_port_list(portList);
		goto err1;

	}else{

		if(openComport(port, 9600, 8, SP_PARITY_NONE, 2, SP_FLOWCONTROL_DTRDSR, &portHandle)) goto err1;

		int rett = is34401A(portHandle);
		if(rett == 0){

			printf("[ERROR]Target is not HPAK 34401A.\n");
			goto err2;

		}else if(rett < 0){

			printf("[ERROR]is34401A error.\n");
			goto err2;
		}
	}

foundPort:
	unsigned int firmVer;
	unsigned int ioVer;
	unsigned int frontVer;
	if(hp34401AGetIDN(portHandle, NULL, NULL, &firmVer, &ioVer, &frontVer)) goto err2;

	if(!isFile(*dumpPath)){
		char *dumpPathBuf = realloc(*dumpPath, strlen(*dumpPath) + sizeof("\\caldump_XX-XX-XX.bin"));
		if(!dumpPathBuf) goto err2;
		*dumpPath = dumpPathBuf;

		sprintf(*dumpPath + strlen(*dumpPath), "\\caldump_%02u-%02u-%02u.bin", firmVer, ioVer, frontVer);
	}

	printf("[INFO]Output Calibration file path = %s\n", *dumpPath);

	FILE *filehandle;
	if(!(filehandle = fopen(*dumpPath, "wb"))){
		printf("[ERROR]dump file create error, errno = %d, strerror = %s\n", errno, strerror(errno));
		goto err2;
	}

	if(sendCommand(portHandle, "SYST:RWL")) goto err3;

	if(printText(portHandle, 0, "CALDUMP STRT")) goto err3;
	if(sendCommand(portHandle, "SYST:BEEP")) goto err3;
	if(sendCommand(portHandle, "SYST:BEEP")) goto err3;

	sleep(2);

	int chksumTablePos = 0;
	unsigned short chksumVal = 0;
	int chksumCompLen = 0;
	for(long memCnt = 0; memCnt < 256; memCnt++){

		printf("[INFO]dumped:%3d%%\r", (int)(((float)memCnt / 255) * 100));
		if(printText(portHandle, 0, "CALDUMP %3d%%", (int)(((float)memCnt / 255) * 100))) goto err3;

		char sendCommandBuf[30];
		sprintf(sendCommandBuf, "DIAG:PEEK? -1,%ld,0", memCnt);

		char *readBuf = NULL;
		if(sendCommandWithRead(portHandle, sendCommandBuf, 0, &readBuf)) goto err3;

		unsigned short readData = strtol(readBuf, NULL, 10);

		if(chksumTablePos < ((firmVer >= 07) ? (sizeof(hp34401ACalChksumTableLater) / sizeof(hp34401ACalChksumTableLater[0])) : (sizeof(hp34401ACalChksumTableEarly) / sizeof(hp34401ACalChksumTableEarly[0]))) &&
			memCnt * 2 == ((firmVer >= 07) ? hp34401ACalChksumTableLater[chksumTablePos] : hp34401ACalChksumTableEarly[chksumTablePos])){

			if(chksumVal + chksumCompLen != readData){

				printf("\n[ERROR]checksum does not match. Val = 0x%04X, Expected = 0x%04X\n", chksumVal + chksumCompLen, readData);
				printText(portHandle, 0, "CHKSUM   ERR", (int)(((float)memCnt / 255) * 100));

				sendCommand(portHandle, "SYST:BEEP");
				sendCommand(portHandle, "SYST:BEEP");
				sendCommand(portHandle, "SYST:BEEP");

				sleep(2);

				sendCommand(portHandle, "SYST:BEEP");
				sendCommand(portHandle, "SYST:LOC");

				goto err3;
			}
			chksumTablePos++;
			chksumVal = 0;
			chksumCompLen = 0;
		}else{
			chksumVal += readData;
			chksumCompLen++;
		}

		if(fwrite(&readData, 1, 2, filehandle) < 1){
			printf("\n[ERROR]fwrite error.\n");
		}

		free(readBuf);
	}

	printf("\n[INFO]finished dump.\n");

	if(printText(portHandle, 0, "CALDUMP END")) goto err3;
	if(sendCommand(portHandle, "SYST:BEEP")) goto err3;
	if(sendCommand(portHandle, "SYST:BEEP")) goto err3;

	sleep(2);

	if(sendCommand(portHandle, "SYST:BEEP")) goto err3;
	if(sendCommand(portHandle, "SYST:LOC")) goto err3;

	ret = 0;
err3:
	fclose(filehandle);
err2:
	if(sp_close(portHandle) < 0) { printf("[ERROR]COM port close error.\n"); ret = -1; goto err1; }
	sp_free_port(portHandle);
	if(portList) sp_free_port_list(portList);
	printf("[INFO]COM port = %s, closed.\n", port);
err1:
	return ret;
}

int flashCal(char *dumpPath, char *port)
{
	return 0;
}

//functions
int openComport(char *portName, int baud, int bits, enum sp_parity parity, int sbits, enum sp_flowcontrol fcontrol, struct sp_port **port)
{
	if(sp_get_port_by_name(portName, port) < 0){
		printf("[ERROR]COM port = %s, error.\n", portName);
		return -1;
	}

	if(sp_open(*port, SP_MODE_READ_WRITE) < 0){
		printf("[ERROR]COM port = %s, open error.\n", portName);
		return -2;
	}

	if(sp_set_baudrate(*port, baud) < 0){
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

	if(sp_set_stopbits(*port, sbits) < 0){
		printf("[ERROR]failed to set stopbits.\n");
		return -6;
	}

	if(sp_set_flowcontrol(*port, fcontrol) < 0){
		printf("[ERROR]failed to set flowcontrol setting.\n");
		return -7;
	}

	if(sp_flush(*port, SP_BUF_BOTH) < 0){
		printf("[ERROR]failed to sp_flush.\n");
		return -8;
	}

	printf("[INFO]COM port = %s, opened.\n", portName);
	return 0;
}

int sendCommand(struct sp_port *port, char *command)
{
	int ret = hp34401ASendCommand(port, command);
	if(ret < 0) printf("\n[ERROR]hp34401ASendCommand error.\n");
	return ret;
}

int sendCommandWithRead(struct sp_port *port, char *command, size_t delay_read_data, char **readData)
{
	int ret = hp34401ASendCommandWithRead(port, command, delay_read_data, readData);
	if(ret != 0) printf("\n[ERROR]hp34401ASendCommandWithRead error.\n");
	return ret;
}

int printText(struct sp_port *port, size_t scrollDelay, char *format, ...)
{
	va_list vList;
	va_start(vList, format);

	int ret = hp34401APrintTextV(port, scrollDelay, format, vList);

	if(ret != 0) printf("\n[ERROR]hp34401APrintTextV error.\n");

	va_end(vList);
	return ret;
}
