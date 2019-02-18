/** \file 
\brief This tool convert 3w files created by XYZware to GCODE and vice versa.


*/


#include "dV3wConverter.h"

#pragma comment(lib, "Shlwapi")


/// Buffer for AES block from 3w file 
BYTE bChunk[CHUNK_LEN]; 



/** @} */ // end of 3w group

//----------------------------------------------------------------------------------------------------------
// Private function
void OnExitCleaning();
void SetMode(BYTE bNewMode, BYTE* pbMode);

//----------------------------------------------------------------------------------------------------------


DWORD InputFileSize; //Input file size in bytes.
LARGE_INTEGER FileSize; // LARGE_INTEGER structure that receives the file size, in bytes from GetFileSizeEx .
DWORD Read_Len = 0; ///< Number of bytes read in last read operation
DWORD Read_TotalSize = 0; ///< Whole number of bytes read in all read operations
DWORD Write_Len = 0; ///< Number of bytes written in last write operation
DWORD Write_TotalSize = 0; ///< Whole number of bytes written in all write operations
uint64_t GcodeLength = 0;

PBYTE InputBuffer = NULL;
PBYTE pOutputBuffer = NULL;

HANDLE hInputFile = INVALID_HANDLE_VALUE; // Input file handle
HANDLE hPrefixFile = INVALID_HANDLE_VALUE; // Prefix file handle
HANDLE hOutputFile = INVALID_HANDLE_VALUE; // Output file handle

BYTE ExOK = 0;
BOOL Success;

extern int Status ; ///< State of program execution.


int wmain ( int argc, TCHAR *argv[])
{		
	int iReturn = 0;

	BYTE bMode = 0;
	BYTE bOverwrite = 0;
	BYTE bPrint = 0;

	DCB dcb; // Defines the control setting for a serial communications device.

	#define m3W 1
	#define mGCODE 2
	#define mPRINT_3W 3
	#define mPRINT_GCODE 4

	TCHAR *InputFileName = NULL;   // Pointer to input file name 
	TCHAR *OutputFileName = NULL;  // Pointer to output file name
	TCHAR *PrefixFileName = NULL;  // Pointer to prefix file name

	atexit(OnExitCleaning);

	if ((argc < 4) || (argc > 6)) {
		Status = IN_ERROR;
		return NO_ERROR;
	}

	// Testing conversion switch
	if (_wcsicmp(argv[1], L"/w") == 0) SetMode(m3W, &bMode);
	if (_wcsicmp(argv[2], L"/w") == 0) SetMode(m3W, &bMode);
	if (_wcsicmp(argv[2], L"/g") == 0) SetMode(mGCODE, &bMode);
	if (_wcsicmp(argv[1], L"/g") == 0) SetMode(mGCODE, &bMode);

	// Testing printing switch
	if (_wcsicmp(argv[1], L"/pw") == 0) { 
		SetMode(mPRINT_3W, &bMode);
	}
	if (_wcsicmp(argv[2], L"/pw") == 0) {
		SetMode(mPRINT_3W, &bMode);
	}
	if (_wcsicmp(argv[1], L"/pg") == 0) {
		SetMode(mPRINT_GCODE, &bMode);
	}
	if (_wcsicmp(argv[2], L"/pg") == 0) {
		SetMode(mPRINT_GCODE, &bMode);
	}

	// No mode set
	if (bMode == 0) {
		Status = IN_ERROR;
		return NO_ERROR;
	}
	
	// Testing overwrite switch
	if (_wcsicmp(argv[1], L"/y") == 0) bOverwrite = 1;
	if (_wcsicmp(argv[2], L"/y") == 0) {
		if (bOverwrite == 1) {
			Status = IN_ERROR;
			return NO_ERROR;
		}
		bOverwrite = 1;
	}

	// Overwrite not allowed in print mode
	if( (bOverwrite == 1)&&((bMode == mPRINT_GCODE)||(bMode == mPRINT_3W)) ){
		Status = IN_ERROR;
		return NO_ERROR;
	}
	
	switch (argc) {
		case 4: 
			// Input and output file name specified in command line, no /Y switch.
			// dV3wConvertre ([/W] | [/G] | [/PW] | [/PG])  Input_File Output_File
			InputFileName = argv[2];
			OutputFileName = argv[3];
		break;
		case 5:
			if (bOverwrite) {
			// Input, output file name and /Y switch specified in command line.
			// dV3wConvertre ([/W] | [/G] | [/PW] | [/PG]) /Y Input_File Output_File
				InputFileName = argv[3];
				OutputFileName = argv[4];
			}
			else {
				// Input, output and prefix file name specified in command line, no /Y switch.
				// dV3wConvertre ([/W] | [/G] | [/PW] | [/PG])  Input_File Output_File Prefix_File
				InputFileName = argv[2];
				OutputFileName = argv[3];
				PrefixFileName = argv[4];
			}
		break;
		case 6:
			// Input, output, prefix file name and /Y switch specified in command line.
			// dV3wConvertre ([/W] | [/G] | [/PW] | [/PG]) /Y Input_File Output_File Prefix_File
			InputFileName = argv[3];
			OutputFileName = argv[4];
			PrefixFileName = argv[5];
			break;
		default:
			Status = IN_ERROR;
			return NO_ERROR;
		break;
	}

	// Input file can't be read.
	if( (InputFileName != NULL) && (_wcsicmp(InputFileName, L"-") != 0) && (_waccess_s(InputFileName, 4) != 0) ){
		fwprintf_s(stderr, L"\nInput file \"%s\" can't be read.\n", InputFileName);
		Status = IN_ERROR;
		return IN_ERROR;
	}

	if ((OutputFileName != NULL) && (_wcsicmp(OutputFileName, L"-") != 0) ) {
		// No overwrite switch set but output file exist.
		if ((bOverwrite == 0) && (_waccess_s(OutputFileName, 0) == 0)) {
			fwprintf_s(stderr, L"\nOutput \"%s\" file exis. Use /Y switch to overwrite it.\n", OutputFileName);
			Status = IN_ERROR;
			return IN_ERROR;
		}
		// Overwrite switch set but output file exist and can't be written.
		if ((bOverwrite == 1) && (_waccess_s(OutputFileName, 0) == 0) && (_waccess_s(OutputFileName, 2) != 0)) {
			fwprintf_s(stderr, L"\nOutput \"%s\" file can't be overwritten.\n", OutputFileName);
			Status = IN_ERROR;
			return IN_ERROR;
		}
	}

	if (PrefixFileName != NULL) {
		if (bMode == m3W) {
			// No overwrite switch set but prefix file exist.
			if ((bOverwrite == 0) && (_waccess_s(PrefixFileName, 0) == 0)) {
				fwprintf_s(stderr, L"\nPrefix file \"%s\" exis. Use /Y switch to overwrite it.\n", PrefixFileName);
				Status = IN_ERROR;
				return IN_ERROR;
			}

			// Overwrite switch set but prefix file exist and can't be written.
			if ((bOverwrite == 1) && (_waccess_s(PrefixFileName, 0) == 0) && (_waccess_s(PrefixFileName, 2) != 0)) {
				fwprintf_s(stderr, L"\nPrefix file \"%s\" can't be overwritten.\n", PrefixFileName);
				Status = IN_ERROR;
				return IN_ERROR;
			}
		}else {
			// Prefix file can't be read.
			if (_waccess_s(PrefixFileName, 4) != 0) {
				fwprintf_s(stderr, L"\nPrefix file \"%s\" can't be read.\n", PrefixFileName);
				Status = IN_ERROR;
				return IN_ERROR;
			}
		}
	}

	if ((_wcsicmp(OutputFileName, L"-") != 0)) {
		fwprintf_s(stdout, L"----\n");
		fwprintf_s(stdout, L"Input File:  %s\n", InputFileName);
		fwprintf_s(stdout, L"Output File: %s\n", OutputFileName);
		if(PrefixFileName != NULL) fwprintf_s(stdout, L"Prefix File: %s\n", PrefixFileName);
		fwprintf_s(stdout, L"----\n");
	}

	// Opening input file.
	if ((_wcsicmp(InputFileName, L"-") != 0)) {
		hInputFile = CreateFileW(InputFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	} else {
		hInputFile = GetStdHandle(STD_INPUT_HANDLE);
	}
	if (hInputFile == INVALID_HANDLE_VALUE) {
		fwprintf_s(stderr, L"Cannot open input file!\n");
		Status = IO_ERROR;
		return IO_ERROR;
	}

	// Output file.
	if ((_wcsicmp(OutputFileName, L"-") != 0)) {
		// Printing
		if ((bMode == mPRINT_3W) || (bMode == mPRINT_GCODE)) {
			// Baud Rate
			DWORD BaudRate = CBR_115200;
			TCHAR *pBaudRate;

			pBaudRate = wcsstr(OutputFileName, L":");
			if (pBaudRate != NULL) {
				BaudRate = _wtoi( (pBaudRate + 1) );
				if (BaudRate == 0) {
					fwprintf_s(stderr, L"Wrong baud rate \"%s\", using 115200.\n", (pBaudRate + 1 ));
					BaudRate = CBR_115200;
				}
			}
			
			// Opening com port
			hOutputFile = CreateFile(OutputFileName, GENERIC_READ | GENERIC_WRITE,
				0,      //  must be opened with exclusive-access
				NULL,   //  default security attributes
				OPEN_EXISTING, //  must use OPEN_EXISTING
				0,      //  not overlapped I/O
				NULL); //  hTemplate must be NULL for comm devices
			if (hOutputFile == INVALID_HANDLE_VALUE)
			{
				//  Handle the error.
				fwprintf_s(stderr, L"Can't open COM port. Operation failed with error %d.\n", GetLastError());
				Status = COM_ERROR;
				return COM_ERROR;
			}

			//  Initialize the DCB structure.
			SecureZeroMemory(&dcb, sizeof(DCB));
			dcb.DCBlength = sizeof(DCB);

			//  Build on the current configuration by first retrieving all current
			//  settings.
			if ((GetCommState(hOutputFile, &dcb)) == 0 )
			{
				fwprintf_s(stderr, L"GetCommState failed with error %d.\n", GetLastError());
				Status = COM_ERROR;
				return COM_ERROR;
			}

			//  Fill in some DCB values and set the com state
			dcb.BaudRate = BaudRate;       //  baud rate
			dcb.ByteSize = 8;              //  data size, xmit and rcv
			dcb.Parity = NOPARITY;         //  parity bit
			dcb.StopBits = ONESTOPBIT;     //  stop bit

			if ((SetCommState(hOutputFile, &dcb)) == 0)
			{
				fwprintf_s(stderr, L"SetCommState failed with error %d.\n", GetLastError());
				Status = COM_ERROR;
				return COM_ERROR;
			}
			
			COMMTIMEOUTS tTimeout;
			if ((GetCommTimeouts(hOutputFile, &tTimeout)) == 0) {
				fwprintf_s(stderr, L"GetCommTimeouts failed with error %d.\n", GetLastError());
				Status = COM_ERROR;
				return COM_ERROR;
			}

			char ComBuffer[20];
			DWORD ComLen;

			tTimeout.ReadTotalTimeoutConstant = 1;
			tTimeout.ReadIntervalTimeout = 0;
			tTimeout.ReadTotalTimeoutMultiplier = 0;

			if ((SetCommTimeouts(hOutputFile, &tTimeout)) == 0) {
				fwprintf_s(stderr, L"SetCommTimeouts failed with error %d.\n", GetLastError());
				Status = COM_ERROR;
				return COM_ERROR;
			}

			WriteFile(hOutputFile, "\n\n", (DWORD)strlen("\n\n"), &ComLen, NULL);
			ReadFile(hOutputFile, ComBuffer, 20, &ComLen, NULL);
			
			tTimeout.ReadTotalTimeoutConstant = 500;

			if( (SetCommTimeouts(hOutputFile, &tTimeout)) == 0) {
				fwprintf_s(stderr, L"SetCommTimeouts failed with error %d.\n", GetLastError());
				Status = COM_ERROR;
				return COM_ERROR;
			}
		}
		else {
			// Creating disk file
			if (bOverwrite) {
				// Overwrite if exist
				hOutputFile = CreateFileW(OutputFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			}
			else {
				// Create new
				hOutputFile = CreateFileW(OutputFileName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			}
		}
	} else {
		// Opening StdOut
		hOutputFile = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	if (hOutputFile == INVALID_HANDLE_VALUE) {
		fwprintf_s(stderr, L"Can't create output file: \"%s\"!\n", OutputFileName);
		Status = IO_ERROR;
		return IO_ERROR;
	}

	// Opening prefix file
	if (PrefixFileName != NULL) {
		if (bMode == m3W) {
			if (bOverwrite) {
				hPrefixFile = CreateFileW(PrefixFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			}
			else {
				hPrefixFile = CreateFileW(PrefixFileName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			}
		}
		if (bMode == mGCODE) {
			hPrefixFile = CreateFileW(PrefixFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		}

		if (hPrefixFile == INVALID_HANDLE_VALUE) {
			fwprintf_s(stderr, L"Cannot open prefix file: %s\n", OutputFileName);
			Status = IO_ERROR;
			return IO_ERROR;
		}
	}

	//  Get input file size.
	Success = GetFileSizeEx(hInputFile, &FileSize);
	if ((!Success) || (FileSize.QuadPart > 0xFFFFFFFF))
	{
		fwprintf_s(stderr, L"Cannot get input file size or file is larger than 4GB.\n");
		Status = IO_ERROR;
		return IO_ERROR;
	}
	InputFileSize = FileSize.LowPart;

	//  Allocation memory for 3w/GCODE content.
	InputBuffer = (PBYTE)malloc(InputFileSize);
	if (!InputBuffer)
	{
		fwprintf_s(stderr, L"Cannot allocate memory for input buffer.\n");
		Status = IO_ERROR;
		return (IO_ERROR);
	}

	// Reading input file
	if ((ReadFile(hInputFile, InputBuffer, InputFileSize, &Read_Len, NULL)) == FALSE) {
		fwprintf_s(stderr, L"Cannot read from input file!\n");
		Status = IO_ERROR;
		return IO_ERROR;
	}
	if ((CloseHandle(hInputFile)) != 0) hInputFile = INVALID_HANDLE_VALUE;
	Read_TotalSize = Read_Len;
	if (Read_Len != InputFileSize) {
		fwprintf_s(stderr, L"Error when reading from input file!\n");
		Status = IO_ERROR;
		return IO_ERROR;
	}

	if ((bMode == mGCODE) || (bMode = mPRINT_GCODE)) {
		for (DWORD i = 0; i < InputFileSize; i++) {
			if (InputBuffer[i] == 0x0A) InputBuffer[i] = 0x0D;
		}
	}

	switch (bMode) {
		case mGCODE:			
			if (PrefixFileName != NULL) {
				if (iReturn = dV3w_Encode(InputBuffer, InputFileSize, &pOutputBuffer, &GcodeLength)) return(iReturn);

				ReadFile(hPrefixFile, bChunk, OFFSET, &Read_Len, NULL);
				if (Read_Len != OFFSET) {
					fwprintf_s(stderr, L"Reading from prefix file failed!\n");
					Status = IO_ERROR;
					return (IO_ERROR);
				}
				if ((CloseHandle(hPrefixFile)) != 0) hPrefixFile = INVALID_HANDLE_VALUE;

				if (!WriteFile(hOutputFile, bChunk, Read_Len, &Write_Len, NULL)) {
					fwprintf_s(stderr, L"Writing to output file failed!\n");
					Status = IO_ERROR;
					return (IO_ERROR);
				}
			}
			else {
				if (iReturn = dV3w_Encode(InputBuffer + OFFSET, InputFileSize - OFFSET, &pOutputBuffer, &GcodeLength)) return(iReturn);

				Read_Len = OFFSET;
				if (!WriteFile(hOutputFile, InputBuffer, Read_Len, &Write_Len, NULL)) {
					fwprintf_s(stderr, L"Writing to output file failed!\n");
					Status = IO_ERROR;
					return (IO_ERROR);
				}
				
			}
			if (Read_Len != Write_Len) {
				fwprintf_s(stderr, L"Writing to output file failed!\n");
				Status = IO_ERROR;
				return (IO_ERROR);
			}

			if (!WriteFile(hOutputFile, pOutputBuffer, (DWORD)GcodeLength, &Write_Len, NULL)) {
				fwprintf_s(stderr, L"Writing to output file failed!\n");
				Status = IO_ERROR;
				return (IO_ERROR);
			}
			if (GcodeLength != Write_Len) {
				fwprintf_s(stderr, L"Writing to output file failed!\n");
				Status = IO_ERROR;
				return (IO_ERROR);
			}

			break;
		case m3W:
		case mPRINT_3W:
			
			if (hPrefixFile != INVALID_HANDLE_VALUE) {
				if (!WriteFile(hPrefixFile, InputBuffer, OFFSET, &Write_Len, NULL)) {
					fwprintf_s(stderr, L"Writing to prefix file failed!\n");
					Status = IO_ERROR;
					return (IO_ERROR);
				}
				if ((CloseHandle(hPrefixFile)) != 0) hPrefixFile = INVALID_HANDLE_VALUE;
				if (Write_Len != OFFSET) {
					fwprintf_s(stderr, L"Writing to prefix file failed!\n");
					Status = IO_ERROR;
					return (IO_ERROR);
				}
			}

			if(iReturn = dV3w_Decode(InputBuffer, InputFileSize, &pOutputBuffer, &GcodeLength)) exit(iReturn);
			

			if (bMode == m3W) {
				if (!WriteFile(hOutputFile, pOutputBuffer, (DWORD)GcodeLength, &Write_Len, NULL)) {
					fwprintf_s(stderr, L"Writing to output file failed!\n");
					Status = IO_ERROR;
					return (IO_ERROR);
				}
			}else {
				dV3w_Print(hOutputFile, pOutputBuffer, GcodeLength);
			}
			break;
		case mPRINT_GCODE:
			dV3w_Print(hOutputFile, InputBuffer, InputFileSize);
			break;
	}

	free(InputBuffer);
	InputBuffer = NULL;

	free(pOutputBuffer);
	pOutputBuffer = NULL;

	Status = NO_ERROR;
	return NO_ERROR;

}


//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//
// Function try to close all opened resource.
// This function is called on exit, they try to determine opened resource and close it.
//
void OnExitCleaning() {
	
	if (Status == IN_ERROR) {
		fwprintf_s(stderr, L"\n\nUsage:\nConverting: \n");
		fwprintf_s(stderr, L"  dV3wConvertre ([/W] | [/G]) [/Y] (<input file> | [-]) (<output file> | [-]) [prefix file]\n\n");
		fwprintf_s(stderr, L"Printing: \n");
		fwprintf_s(stderr, L"  dV3wConvertre ([/PW] | [/PG]) (<input file> | [-]) <com port[:com speed]>\n\n");
		fwprintf_s(stderr, L"Input or/and output file can be \"-\" to read or write to stdin or stdout.\n\n");

		fwprintf_s(stderr, L"Example:\nExtracting GCODE from 3w file:\n");
		fwprintf_s(stderr, L"  dV3wConvertre /W [/Y] Input_3w_File Output_GCODE_File [Output_Prefix_File]\n\n");
		fwprintf_s(stderr, L"Creating 3w file from GCODE:\n");
		fwprintf_s(stderr, L"  dV3wConvertre /G [/Y] Input_GCODE_File Output_3w_File [Input_Prefix_File]\n");
		fwprintf_s(stderr, L"\nFor creating 3w file the 3w file prefix is needed.\n");
		fwprintf_s(stderr, L"If the 3w file prefix is not specified on the command line it is assumed that it is given at beginning of input file.\n\n");
		fwprintf_s(stderr, L"Printing 3W file:\n");
		fwprintf_s(stderr, L"  dV3wConvertre  /PW Input_3w_File com12\n\n");
		fwprintf_s(stderr, L"Printing GCODE file:\n");
		fwprintf_s(stderr, L"  dV3wConvertre  /PG Input_GCODE_File com12\n");
	}

	if ( InputBuffer != NULL )   free( InputBuffer );
	if ( pOutputBuffer != NULL)  free( pOutputBuffer );

	if ((hInputFile  != INVALID_HANDLE_VALUE) && (hInputFile  != NULL)) CloseHandle(hInputFile);
	if ((hOutputFile != INVALID_HANDLE_VALUE) && (hOutputFile != NULL)) CloseHandle(hOutputFile);
	if ((hPrefixFile != INVALID_HANDLE_VALUE) && (hPrefixFile != NULL)) CloseHandle(hPrefixFile);

}

//----------------------------------------------------------------------------------------------------------

void SetMode(BYTE bNewMode, BYTE* pbMode) {
	if (*pbMode) {
		Status = MODE_ERROR;
		exit(MODE_ERROR);
	}
	*pbMode = bNewMode;
}

//----------------------------------------------------------------------------------------------------------
