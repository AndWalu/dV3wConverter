#define _STRALIGN_USE_SECURE_CRT 1

#include "dV3wConverter.h"
#include <windows.h>
#include <Commctrl.h>

#pragma comment(lib, "zip")
#pragma comment(lib, "zlib")

/** @defgroup TinyAES The TinyAES library configuration

Tiny AES https://github.com/kokke/tiny-AES-c
*  @{
*/
#define CBC 1 ///< AES library - Enable ECB
#define ECB 0 ///< AES library - Disable ECB
#define CTR 0 ///< AES library - Disable CTR
extern "C" {
#include "..\lib\tiny-AES-c\aes.h"
}
/** @} */ // end of AES group

int Status = WORKING; ///< State of program execution.

struct AES_ctx ctx;

uint8_t key[] = "@xyzprinting.com"; ///< Key for AES encryption of 3w files.
uint8_t iv[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/** @defgroup LIBZIP Libzip is a C library for reading, creating, and modifying zip archives.

Libzip https://libzip.org/
* @{
*/
struct zip_file *ZipFile; ///< libzip - zip file structure
struct zip_stat ZipState; ///< libzip - zip structure state
zip_error_t ZipError; ///< libzip - zip structure last error code.

struct zip *ZipArchive = NULL; ///< libzip - zip archive  
struct zip_source *ZipSource = NULL; ///< libzip - empty zip source
struct zip_source *ZipGcodeSource = NULL; ///< libzip - zip source for GCODE data
										  /** @} */ // end of LIBZIP group

int dV3w_Send(HANDLE hCOM, char* Buffer, WORD sLen, char* Response, char* Info, DWORD ResponseLen = 10);
int dV3w_Status(HANDLE hCOM);

#ifndef _CONSOLE
wchar_t wMsgBuffer[MSGSIZE];
#endif

//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//
// Decode 3w data (InputBuffer) to GCODE format and store it in pGcodeBuffer of length pGcodeLength
//
int dV3w_Decode(PBYTE InputBuffer, DWORD Length, PBYTE* pGcodeBuffer, uint64_t* pGcodeLength, HWND hDlg, int nIDDlgItem)
{
	DWORD i;
	DWORD Chunk = CHUNK_LEN;
	DWORD Cumulated_Pad_Len = 0;
	zip_int64_t Read_Len = 0;
	BYTE Progress = 0;

	/// Default padding block for all AES block from 3w file *except the last one*.
	BYTE bPadding[0x10] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };
	BYTE bPad_Len = 0x10;

	HWND Label = GetDlgItem(hDlg, nIDDlgItem);

	if( (Length - OFFSET)%16 ) {
		ERRORPRINT( L"Probably not a 3w file! Size: %uB", Length);
		Status = AES_ERROR;
		return(AES_ERROR);
	}

	for(i = OFFSET; i < Length; i += CHUNK_LEN){
		if (i + Chunk > Length) Chunk = Length - i;
		AES_init_ctx_iv(&ctx, key, iv);
		AES_CBC_decrypt_buffer(&ctx, (InputBuffer + i), Chunk);

		INFOPRINT( L"Decoding: %6.2f%%", (100.0*(i)) / (Length - OFFSET) );

		bPad_Len = InputBuffer[ i + Chunk - 1];
		if (bPad_Len > 0x10) {
			ERRORPRINT( L"\nCorrupted 3w file!");
			Status = AES_ERROR;
			return(AES_ERROR);
		}
		memset(bPadding, bPad_Len, bPad_Len);
		if (memcmp(&(InputBuffer[i + Chunk - bPad_Len]), bPadding, bPad_Len)) {
			ERRORPRINT( L"\nPadding error!");
			Status = AES_ERROR;
			return(AES_ERROR);
		}

		if( Cumulated_Pad_Len != 0) memmove_s(InputBuffer + i - Cumulated_Pad_Len, Chunk, InputBuffer + i, Chunk);
		Cumulated_Pad_Len += bPad_Len;
	}
	INFOPRINT( L"Decoding: 100.00%%\n");

	zip_error_init(&ZipError);
	// Create source from buffer 
	if ((ZipSource = zip_source_buffer_create((void*)(InputBuffer + OFFSET), Length - OFFSET - Cumulated_Pad_Len, 0, &ZipError)) == NULL)
	{
		ERRORPRINT( L"Can't create zip source: %S\n", zip_error_strerror(&ZipError));
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Open zip archive from source 
	if ((ZipArchive = zip_open_from_source(ZipSource, 0, &ZipError)) == NULL) {
		ERRORPRINT( L"Can't open zip from source: %S\n", zip_error_strerror(&ZipError));
		zip_source_free(ZipSource);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	if (zip_get_num_entries(ZipArchive, 0) != 1) {
		ERRORPRINT( L"Only one compressed file allowed in the 3w file. Find %lli files inside.\n", zip_get_num_entries(ZipArchive, 0));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return (ZIP_ERROR);
	}

	if ((zip_stat_index(ZipArchive, 0, 0, &ZipState)) != 0) {
		ERRORPRINT( L"Can't get content information from zip file.\n");
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	if ((ZipFile = zip_fopen_index(ZipArchive, 0, 0)) == NULL) {
		ERRORPRINT( L"Can't get content from zip file.\n");
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	*pGcodeLength = ZipState.size;

	//  Allocation memory for GCODE content.
	*pGcodeBuffer = (PBYTE)malloc((size_t)(*pGcodeLength));
	if ( *pGcodeBuffer == NULL )
	{
		ERRORPRINT( L"Cannot allocate memory for GCODE buffer.\n");
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = IO_ERROR;
		return(IO_ERROR);
	}

	Read_Len = zip_fread(ZipFile, *pGcodeBuffer, *pGcodeLength);
	if (Read_Len < 0) {
		ERRORPRINT( L"Reading from zip file failed.\n");
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}
	
	if (Read_Len != *pGcodeLength) {
		ERRORPRINT( L"Reading from zip file are incomplete!\n");
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}


	zip_source_free(ZipSource);
	zip_close(ZipArchive);

	return NO_ERROR;
}

//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//
// Encode GCODE data (InputBuffer) to 3w format and store it in p3wBuffer of length p3wLength
//
int dV3w_Encode(PBYTE InputBuffer, DWORD InputLength, PBYTE* p3wBuffer, uint64_t* p3wLength, HWND hDlg, int nIDDlgItem)
{
	/// Default padding block for all AES block from 3w file *except the last one*.
	BYTE bPadding[0x10] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };
	BYTE bPad_Len = 0x10;

	HWND Label = GetDlgItem(hDlg, nIDDlgItem);

	zip_error_init(&ZipError);

	// Create empty source  for ZIP archive
	if ((ZipSource = zip_source_buffer_create(NULL, 0, ZIP_CREATE, &ZipError)) == NULL)
	{
		ERRORPRINT( L"Can't create zip source: %S\n", zip_error_strerror(&ZipError));
		zip_source_free(ZipSource);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Open ZIP archive from empty source
	if ((ZipArchive = zip_open_from_source(ZipSource, 0, &ZipError)) == NULL) {
		ERRORPRINT( L"Can't open zip from source: %S\n", zip_error_strerror(&ZipError));
		zip_source_free(ZipSource);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Create ZIP source from GCODE file
	if ((ZipGcodeSource = zip_source_buffer(ZipArchive, InputBuffer, InputLength, 0)) == NULL)
	{
		ERRORPRINT( L"Can't create zip source from GCODE file: %S\n", zip_error_strerror(zip_get_error(ZipArchive)));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Add the GCODE file to a ZIP archive
	zip_int64_t zipFileIndex;
	if ((zipFileIndex = zip_file_add(ZipArchive, "temp.gcode", ZipGcodeSource, 0)) < 0) {
		ERRORPRINT( L"Can't add GCODE file to zip archive: %S\n", zip_error_strerror(zip_get_error(ZipArchive)));
		zip_source_free(ZipGcodeSource);
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Set GCODE file compression
	if ((zip_set_file_compression(ZipArchive, zipFileIndex, ZIP_CM_DEFLATE, 5)) != 0) {
		ERRORPRINT( L"Can't set file compression: %S\n", zip_error_strerror(zip_get_error(ZipArchive)));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Keep ZIP archive source
	zip_source_keep(ZipSource);

	// Write ZIP archive changes to ZIP source
	if ((zip_close(ZipArchive)) != 0) {
		ERRORPRINT( L"Can't close zip archive: %S\n", zip_error_strerror(zip_get_error(ZipArchive)));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}
	ZipArchive = NULL;

	// Get ZIP source properties
	if (zip_source_stat(ZipSource, &ZipState) < 0) {
		ERRORPRINT( L"Can't state source: %S\n", zip_error_strerror(zip_source_error(ZipGcodeSource)));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	// Reading ZIP fille source
	if (zip_source_open(ZipSource) != 0) {
		ERRORPRINT( L"Can't open ZIP source for reading: %S\n", zip_error_strerror(zip_source_error(ZipGcodeSource)));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}

	zip_int64_t  Zip_Read_Len;
	if ((Zip_Read_Len = zip_source_read(ZipSource, InputBuffer, ZipState.comp_size)) == -1) {
		ERRORPRINT( L"Can't read from ZIP source: %S\n", zip_error_strerror(zip_source_error(ZipGcodeSource)));
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = ZIP_ERROR;
		return(ZIP_ERROR);
	}
	
	// Calculating room for encrypted 3 w data. Number of chunk * chunk size
	*p3wLength = CHUNK_LEN * ( 1 + Zip_Read_Len /(CHUNK_LEN - PAD_LEN) );

	//  Allocation memory for GCODE content. 
	*p3wBuffer = (PBYTE)malloc((size_t)(*p3wLength));
	if (*p3wBuffer == NULL)
	{
		ERRORPRINT( L"Cannot allocate memory for 3w buffer.\n");
		zip_source_free(ZipSource);
		zip_close(ZipArchive);
		Status = IO_ERROR;
		return(IO_ERROR);
	}
	
	uint32_t Part;
	PBYTE Buffer;
	Buffer = *p3wBuffer;
	for (size_t i = 0; i < (size_t)Zip_Read_Len; i += (CHUNK_LEN - PAD_LEN))
	{
		if ((i + CHUNK_LEN - 0x10) <= (size_t)Zip_Read_Len) {
			memcpy(Buffer, (InputBuffer + i), (CHUNK_LEN - 0x10));
			memcpy((Buffer + CHUNK_LEN - 0x10), bPadding, 0x10);
			Part = CHUNK_LEN;
		}
		else {
			Part = (uint32_t)(Zip_Read_Len - i);
			memcpy(Buffer, (InputBuffer + i), Part);
			// Padding calculation for last chunk
			bPad_Len = 16 - Part % 16;
			for (size_t i = 0; i < bPad_Len; i++)
			{
				bPadding[i] = bPad_Len;
			}
			memcpy((Buffer + Part), bPadding, bPad_Len);
			Part += bPad_Len;
		}

		AES_init_ctx_iv(&ctx, key, iv);
		AES_CBC_encrypt_buffer(&ctx, Buffer, Part);

		INFOPRINT( L"Encoding: %6.2f%%", (100.0*i) /Zip_Read_Len);

		Buffer += Part;
	}
	INFOPRINT( L"Encoding: 100.00%%");

	zip_source_free(ZipSource);
	zip_close(ZipArchive);

	if (*p3wLength >= (uint64_t)(Buffer - *p3wBuffer)) {
		*p3wLength = (Buffer - *p3wBuffer);
	}
	else {
		ERRORPRINT( L"Buffer for output 3w data: size error. %llu - %llu\n", (uint64_t)*p3wLength, (uint64_t)(Buffer - *p3wBuffer));
		Status = IO_ERROR;
		return(IO_ERROR);
	}

	return NO_ERROR;
}

//----------------------------------------------------------------------------------------------------------

int dV3w_Print(HANDLE hCOM, PBYTE GcodeBuffer, uint64_t GcodeLength, HWND hDlg, int nIDDlgItem) {
	int sLen;
	char Buffer[PRINT_CHUNK_LEN + 5];

	//DEBUGBOX(L"GcodeLength %llu\n", GcodeLength);

	HWND Label = GetDlgItem(hDlg, nIDDlgItem);

	for (uint64_t i = 0; i < GcodeLength; i++) {
		if (GcodeBuffer[i] == 0x0A) GcodeBuffer[i] = 0x0D;

		INFOPRINT(L"%cSending:  %lu%%", 13, i);
	}

	// Send status command
	if (Status = dV3w_Status(hCOM)) {
		return Status;
	}

	// Send file command
	sLen = sprintf_s(Buffer, PRINT_CHUNK_LEN, "XYZ_@3D:4\n");
	if (Status = dV3w_Send(hCOM, Buffer, sLen, (char*)"OFFLINE_OK", (char*)"The printer is not responding to the command.", 10)) {
		return Status;
	}

	// File description
	sLen = sprintf_s(Buffer, PRINT_CHUNK_LEN, "M1:Test,%llu,1.3.49,EE1_OK,EE2_OK", GcodeLength);
	if( Status = dV3w_Send(hCOM, Buffer, sLen, (char*)"OFFLINE_OK", (char*)"The printer rejects a file description.", 10)) {
		return Status;
	}

	// File content
	DWORD Sum;
	PBYTE pBy = NULL;
	

	for (uint64_t i = 0; i < GcodeLength; i += PRINT_CHUNK_LEN) {
		sLen = PRINT_CHUNK_LEN;
		if ((i + PRINT_CHUNK_LEN) > GcodeLength) {
			sLen = (int)(GcodeLength - i);
		}
		Sum = 0;
		for (int j = 0; j < sLen; j++) {
			Buffer[j] = GcodeBuffer[i + j];
			Sum += GcodeBuffer[i + j];
		}
		pBy = (PBYTE)&Sum;
		for (int k = 0; k < 4; k++) {
			Buffer[sLen + k] = pBy[3 - k];
		}


		INFOPRINT( L"%cSending:  %6.2f%%", 13, (100.0*i) / GcodeLength);
//		DEBUGBOX(L"dV3w_Sending\n");

		if( Status = dV3w_Send(hCOM, Buffer, sLen + 4, (char*)"CheckSumOK", (char*)"The printer rejects a data chunk.", 14)) {
			return Status;
		}
//		DEBUGBOX(L"dV3w_Send\n");

	}
	INFOPRINT( L"%cSending:  100.00%%", 13);	
	fwprintf(stderr, L"\n");
	return 0;
}

//----------------------------------------------------------------------------------------------------------

int dV3w_Send(HANDLE hCOM, char* Buffer, WORD sLen, char* Response, char* Info, DWORD ResponseLen) {
	DWORD Write_Len, Read_Len;
	
	WriteFile(hCOM, Buffer, sLen, &Write_Len, NULL);
	// Reading answer string
	ReadFile(hCOM, Buffer, ResponseLen, &Read_Len, NULL);
	if ((strstr(Buffer, Response)) == NULL) {
		ERRORPRINT( L"\n%S\n", Info);
		Status = PRN_ERROR;
		return(PRN_ERROR);
	}
	// Reading CR/LF
	ReadFile(hCOM, Buffer, 2, &Read_Len, NULL);
	return 0;
}

//----------------------------------------------------------------------------------------------------------

int dV3w_Status(HANDLE hCOM) {
	DWORD Write_Len, Read_Len;
	char Buffer[100];
	int Pos;

	WriteFile(hCOM, "XYZv3/query=a", 13, &Write_Len, NULL);

	while (1) {
		Pos = 0;
		while (Pos < 99) {
			ReadFile(hCOM, &Buffer[Pos], 1, &Read_Len, NULL);
			if (Buffer[Pos] == '\n') {
				Buffer[Pos + 1] = 0;
				break;
			}
			if (Read_Len == 0) break;
			Pos += Read_Len;
		}
		if (Buffer[Pos - 1] == '$') {
			break;
		}
		if (Read_Len == 0) {
			ERRORPRINT(L"\nReading printer status failed.\n");
			Status = PRN_ERROR;
			return(PRN_ERROR);
		}
	}
	return 0;
}


//----------------------------------------------------------------------------------------------------------

BOOL dV3w_Is3w() {
	char Mark[] = "3DPFNKG13WTW";

	return TRUE;
}

//----------------------------------------------------------------------------------------------------------

