#include <windows.h>
#include <stdio.h>

#define forever for (;;)
	
#define MAX_PROCESSING 4

#define NUM_WORKERS (MAX_PROCESSING * 2)

#define BUFFER_SIZE 2048

typedef struct AsyncContext {
	OVERLAPPED overlapped;
	CHAR buffer[BUFFER_SIZE];
	DWORD numberOfBytesRead;
	HANDLE file;
} ASYNC_CTX, * PASYNC_CTX;

DWORD WINAPI WorkerThread(LPVOID lpParameter) {
	HANDLE iocp = (HANDLE) lpParameter;
	
	DWORD NumberOfBytes;
	ULONG_PTR CompletionKey;
	LPOVERLAPPED OverlappedPtr;
	
	printf(
		"IOCP: Worker starting (%u)\n", 
		GetCurrentThreadId()
	);
	
	forever {
		
		GetQueuedCompletionStatus(
			iocp,
			&NumberOfBytes,
			&CompletionKey,
			&OverlappedPtr,
			INFINITE
		);
		
		printf(
			"IOCP (%d): %d, %llu, %p\n",
			GetCurrentThreadId(),
			NumberOfBytes,
			CompletionKey,
			OverlappedPtr
		);

		// DO SOMETHING...
		PASYNC_CTX pAsyncCtx = (PASYNC_CTX) CompletionKey;
		CloseHandle(pAsyncCtx->file);
		free(pAsyncCtx);
	}
}

int main(int argc, const char * argv[]) {
	
	HANDLE iocp = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL,
		0,
		MAX_PROCESSING
	);
	
	if (iocp == NULL) {
		fprintf(stderr, "ERR: failed to create IOCP\n");
		exit(4);
	}
	
	for (int nt = 0; nt < NUM_WORKERS; ++nt) {
		CreateThread(
			NULL,
			0,
			WorkerThread,
			iocp,
			0,
			NULL
		);
	}
	
	for (int na = 1; na < argc; ++na) {
		
		printf("INIT: %s\n", argv[na]);
		
		HANDLE file = CreateFile(
			argv[na],
			GENERIC_READ, 
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL
		);
		
		if (file == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "FAIL: %s\n", argv[na]);
			exit(1);
		}

		PASYNC_CTX pAsyncCtx = 
			(PASYNC_CTX) malloc(sizeof (ASYNC_CTX));
		
		memset(pAsyncCtx, 0, sizeof (ASYNC_CTX));
		
		pAsyncCtx->file = file;
		
		// ISTO NÃO ESTÁ A CRIAR UM IOCP
		// Esta operação associa um HANDLE a
		// um IOCP já existente.
		HANDLE assoc = CreateIoCompletionPort(
			file,
			iocp,
			(ULONG_PTR) pAsyncCtx, // CONTEXT INFORMATION
			0
		);
		
		if (assoc == NULL) {
			fprintf(
				stderr,
				"ERR: association failed for %s (%u)\n",
				argv[na],
				GetLastError()
			);
			exit(3);
		}
		
		BOOL res = ReadFile(
			file,
			pAsyncCtx->buffer,
			BUFFER_SIZE,
			&(pAsyncCtx->numberOfBytesRead),
			&(pAsyncCtx->overlapped)
		);
		
		if (res == TRUE) {
			printf("SYNC: operation completed synchronously\n");
			CloseHandle(file);
			free(pAsyncCtx);
		} else {
			DWORD err = GetLastError();
			if (err != ERROR_IO_PENDING) {
				fprintf(stderr, "ERR: error reading %s\n", argv[na]);
				exit(2);
			} else {
				printf("ASYNC: operation ongoing asynchronously\n");
			}
		}
	}
	
	getchar();
	
	return 0;
}