// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


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
} ASYNC_CTX, *PASYNC_CTX;

typedef VOID(*AsyncCallback)(
	LPVOID userCtx,
	DWORD status,
	UINT64 transferedBytes);

HANDLE iocp;

//Constrói a inf. necessária ao suporte de operações assíncronas de cópia de ficheiros.
//Na eventualidade de invocação múltipla apenas a primeira invocação deve ter efeito.
BOOL AsyncInit() {
 
	iocp = CreateIoCompletionPort( // threads que efetuam trabalho tem prioridade, o que facilita o acabar das threads
		INVALID_HANDLE_VALUE,
		NULL,
		0,
		MAX_PROCESSING// max number of threads:
	);

	if (iocp == NULL) {
		fprintf(stderr, "ERR: failed to create IOCP\n");
		return false;
		//exit(4);
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


	return false;
}

//cópia assincrona do ficheiro srcfile para dstfile,na conclusao ou erro invoca "cb" com argumento userctx,o status e o total de bytes transf.
//status = 0 se sucesso, caso contrario status = GestLastError();
// retorn true se sucesso e false se contrário
BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx) {
	// tem de decidir se o ficheiro fechou com sucesso ou nao da seguinte forma:
	// se a leitura retornar zero bits lidos, então é sucesso, se a 
	// há quatro tipos de desfecho, sucesso a ler, sucesso a escrever, erro a ler erro a escrever

	return false;
}

//termina e destrói recursos, se invocacao multipla, apenas a primeira deve ter efeito
//valoriza se se garantir a exec de todas as operacoes de cópia pendentes
VOID AsyncTerminate() {
	//TODO
	//termina a leitura de todos os ficheiros 

}

DWORD WINAPI WorkerThread(LPVOID lpParameter) {
	HANDLE iocp = (HANDLE)lpParameter;

	DWORD NumberOfBytes;
	ULONG_PTR CompletionKey; // serve para destinguir as operacoes
	LPOVERLAPPED OverlappedPtr;

	printf(
		"IOCP: Worker starting (%u)\n",
		GetCurrentThreadId()
	);

	forever{

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
	PASYNC_CTX pAsyncCtx = (PASYNC_CTX)CompletionKey;
	CloseHandle(pAsyncCtx->file);// le e fecha, sem mt conteudo
	free(pAsyncCtx);
	}
}

int main(int argc, const char * argv[]) {

	BOOL inited = AsyncInit();
	if (!inited) exit(4);


	for (int na = 1; na < argc; ++na) {

		printf("INIT: %s\n", argv[na]);

		HANDLE file = CreateFile(
			(LPCWSTR)argv[na],
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
		//desencadear operacoes assync no ficheiro



		PASYNC_CTX pAsyncCtx =
			(PASYNC_CTX)malloc(sizeof(ASYNC_CTX));

		memset(pAsyncCtx, 0, sizeof(ASYNC_CTX));

		pAsyncCtx->file = file;

		// ISTO NÃO ESTÁ A CRIAR UM IOCP
		// Esta operação associa um HANDLE a
		// um IOCP já existente.
		HANDLE assoc = CreateIoCompletionPort(
			file,
			iocp,
			(ULONG_PTR)pAsyncCtx, // CONTEXT INFORMATION
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
			free(pAsyncCtx);//context info
		}
		else {
			DWORD err = GetLastError();
			if (err != ERROR_IO_PENDING) {
				fprintf(stderr, "ERR: error reading %s\n", argv[na]);
				exit(2);
			}
			else {
				printf("ASYNC: operation ongoing asynchronously\n");
			}
		}
	}

	getchar();

	return 0;
}