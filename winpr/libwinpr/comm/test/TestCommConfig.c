/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/handle.h>

int TestCommConfig(int argc, char* argv[])
{
	DCB dcb = { 0 };
	BOOL success = FALSE;
	LPCSTR lpFileName = "\\\\.\\COM1";
	COMMPROP commProp = { 0 };
	struct stat statbuf = { 0 };

	HANDLE hComm =
	    CreateFileA(lpFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hComm && (hComm != INVALID_HANDLE_VALUE))
	{
		(void)fprintf(
		    stderr, "CreateFileA failure: could create a handle on a not yet defined device: %s\n",
		    lpFileName);
		return EXIT_FAILURE;
	}

	if (stat("/dev/ttyS0", &statbuf) < 0)
	{
		(void)fprintf(stderr, "/dev/ttyS0 not available, making the test to succeed though\n");
		return EXIT_SUCCESS;
	}

	success = DefineCommDevice(lpFileName, "/dev/ttyS0");
	if (!success)
	{
		(void)fprintf(stderr, "DefineCommDevice failure: %s\n", lpFileName);
		return EXIT_FAILURE;
	}

	hComm = CreateFileA(lpFileName, GENERIC_READ | GENERIC_WRITE,
	                    FILE_SHARE_WRITE, /* invalid parameter */
	                    NULL, CREATE_NEW, /* invalid parameter */
	                    0, (HANDLE)1234); /* invalid parameter */
	if (hComm != INVALID_HANDLE_VALUE)
	{
		(void)fprintf(
		    stderr, "CreateFileA failure: could create a handle with some invalid parameters %s\n",
		    lpFileName);
		return EXIT_FAILURE;
	}

	hComm = CreateFileA(lpFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (!hComm || (hComm == INVALID_HANDLE_VALUE))
	{
		(void)fprintf(stderr, "CreateFileA failure: %s GetLastError() = 0x%08x\n", lpFileName,
		              GetLastError());
		return EXIT_FAILURE;
	}

	/* TODO: a second call to CreateFileA should failed and
	 * GetLastError should return ERROR_SHARING_VIOLATION */

	dcb.DCBlength = sizeof(DCB);
	success = GetCommState(hComm, &dcb);
	if (!success)
	{
		(void)fprintf(stderr, "GetCommState failure: GetLastError() = Ox%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	(void)fprintf(stderr,
	              "BaudRate: %" PRIu32 " ByteSize: %" PRIu8 " Parity: %" PRIu8 " StopBits: %" PRIu8
	              "\n",
	              dcb.BaudRate, dcb.ByteSize, dcb.Parity, dcb.StopBits);

	if (!GetCommProperties(hComm, &commProp))
	{
		(void)fprintf(stderr, "GetCommProperties failure: GetLastError(): 0x%08x\n",
		              GetLastError());
		return EXIT_FAILURE;
	}

	if ((commProp.dwSettableBaud & BAUD_57600) <= 0)
	{
		(void)fprintf(stderr, "BAUD_57600 unsupported!\n");
		return EXIT_FAILURE;
	}

	if ((commProp.dwSettableBaud & BAUD_14400) > 0)
	{
		(void)fprintf(stderr, "BAUD_14400 supported!\n");
		return EXIT_FAILURE;
	}

	dcb.BaudRate = CBR_57600;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	success = SetCommState(hComm, &dcb);

	if (!success)
	{
		(void)fprintf(stderr, "SetCommState failure: GetLastError() = 0x%x\n", GetLastError());
		return EXIT_FAILURE;
	}

	success = GetCommState(hComm, &dcb);

	if (!success)
	{
		(void)fprintf(stderr, "GetCommState failure: GetLastError() = 0x%x\n", GetLastError());
		return 0;
	}

	if ((dcb.BaudRate != CBR_57600) || (dcb.ByteSize != 8) || (dcb.Parity != NOPARITY) ||
	    (dcb.StopBits != ONESTOPBIT))
	{
		(void)fprintf(stderr,
		              "Got an unexpected value among: BaudRate: %" PRIu32 " ByteSize: %" PRIu8
		              " Parity: %" PRIu8 " StopBits: %" PRIu8 "\n",
		              dcb.BaudRate, dcb.ByteSize, dcb.Parity, dcb.StopBits);
	}

	(void)CloseHandle(hComm);

	return 0;
}
