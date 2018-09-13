
#pragma once

/** @defgroup 3W XYZware 3w file

3w file format description
\image html 3w_file.gif width=1500

Printing: 
- https://forum.voltivo.com/showthread.php?tid=7949
- https://github.com/tbirdsaw/xyz-tools

*  @{
*/

#define CHUNK_LEN 	0x2010 ///< Length of AES block in 3w file (8208)
#define OFFSET 		0x2000 ///< Offset from beginning of 3w file to first AES block (8192)
#define PAD_LEN		0x10 

#define PRINT_CHUNK_LEN	0x27FC ///< Length of GCODE block send to the printer
/** @} */ // end of 3W group



/** @defgroup EXIT State definition, tested on exit
*  @{
*/
#define WORKING		1 //Unexpected exit.
#define IO_ERROR	2 //The IO error occurred.
#define ZIP_ERROR	3 //The ZIP library error occurred.
#define IN_ERROR	4 //Incorrect command line parameters.
#define AES_ERROR	5 //The Tiny AES library error occurred.
#define MODE_ERROR	6 //Wrong mode set in command line?
#define COM_ERROR	7 //COM port related error.
#define PRN_ERROR	8 //Printing related error.
/** @} */ // end of EXIT group

int dV3w_Decode(PBYTE InputBuffer, DWORD Length, PBYTE* pGcodeBuffer, uint64_t* pGcodeLength, HWND hDlg = NULL, int nIDDlgItem = 0);
int dV3w_Encode(PBYTE InputBuffer, DWORD Length, PBYTE* p3wBuffer, uint64_t* p3wLength, HWND hDlg = NULL, int nIDDlgItem = 0);
int dV3w_Print(HANDLE hCOM, PBYTE GcodeBuffer, uint64_t GcodeLength, HWND hDlg = NULL, int nIDDlgItem = 0);

#ifndef _CONSOLE
extern wchar_t wMsgBuffer[];
#endif


#ifdef _CONSOLE
#define ERRORPRINT(...) fwprintf(stderr, __VA_ARGS__)
#define INFOPRINT(...)  fwprintf(stderr, L"%c", 13);   fwprintf(stderr, __VA_ARGS__);
#define DEBUGBOX
#else
  #define MSGSIZE 255

  #define ERRORPRINT(...) swprintf_s(wMsgBuffer, MSGSIZE, __VA_ARGS__);\
		::MessageBox(NULL, wMsgBuffer, L"Notepad++ dV3w plugin error.", MB_OK);
	
  #define INFOPRINT(...)  if(Label != NULL){\
		swprintf_s(wMsgBuffer, MSGSIZE, __VA_ARGS__);\
		SetWindowText( Label, wMsgBuffer);\
		UpdateWindow(hDlg);\
	 }

  #define DEBUGBOX(...) swprintf_s(wMsgBuffer, MSGSIZE, __VA_ARGS__);\
		::MessageBox(NULL, wMsgBuffer, L"Notepad++ dV3w plugin debug msg.", MB_OK);

#endif




/*
https://forum.voltivo.com/showthread.php?tid=7949
It's a serial connection: 115200, 8N1, software flow control.
Also you can use 230400 instead of 115200 to upload the files to the printer. That's what they use in Mac XYZ app.

https://github.com/tbirdsaw/xyz-tools/blob/master/uploader.py

The command set is XYZ_@3D:#, where # is either blank or a number 3-8.
Known Commands:
MACHINE_INFO		'XYZ_@3D:'
SEND_FIRMWARE 		'XYZ_@3D:3'
SEND_FILE 			'XYZ_@3D:4'
MACHINE_LIFE		'XYZ_@3D:5'
EXTRUDER1_INFO		'XYZ_@3D:6'
EXTRUDER2_INFO		'XYZ_@3D:7'
STATUS_INFO 		'XYZ_@3D:8'

XYZWare sends GCode files in chunks of 10236 bytes. There's a 4-byte checksum at the end,
sum of the integer value of all the ASCII characters in the chunk added up.
There's a "pre-header" that's sent before the GCode file is uploaded... it does send the file size to the printer.


*/
