// Main.cpp
//

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include "SHA-256.h"
#define _CRT_SECURE_NO_WARNINGS 

#pragma warning(disable:4996)

//#define TEST_CODE

// Global Variables
unsigned char gkey[65537];
unsigned char *gptrKey = gkey;			// used for inline assembly routines, need to access this way for Visual Studio
char gPassword[256] = "password";
unsigned char gPasswordHash[32];
unsigned char *gptrPasswordHash = gPasswordHash;	// used for inline assembly routines, need to access this way for Visual Studio

FILE *gfptrIn = NULL;
FILE *gfptrOut = NULL;
FILE *gfptrKey = NULL;
char gInFileName[256];
char gOutFileName[256];
char gKeyFileName[256];
int gOp = 0;			// 1 = encrypt, 2 = decrypt
int gNumRounds = 1;
const char * debug = "%d = ebp, \n";


// Prototypes
int sha256(char *fileName, char *dataBuffer, DWORD dataLength, unsigned char sha256sum[32]);

// assembly language to count the number of ASCII letters in a data array
//	numC = number of capital letters
//	numL = number of lowercase letters
//	numO = number of characters that are not a letter
void exCountLetters( char *data, int dataLength, int *numC, int *numL, int *numO )
{
	__asm {
		cld;					// 
		push esi;				// 
		push ecx;				// 
		push ebx;
		mov esi,data;			// 
		mov ecx, dataLength;	// 

LOOP_X1:
		lodsb;					// 
		mov bl,al				// 
		push eax;				// 
		call isLetter;			// function returns a 1 in al if the character passed in is a letter, otherwise al = 0
		add esp,4				// 
		test al,al;				// 
		je lbl_OTHER;			// 

		mov al,bl				// 
		and al,0x20;			// already know it's a letter, if al == 0, then CAP
		je lbl_CAP;
		
		mov	ebx,numL;			// 
		add [ebx],1;			// 
		jmp lbl_NEXT;			// 

lbl_CAP:
		mov ebx,numC;			// 
		add [ebx],1;			// 
		jmp lbl_NEXT;			// 

lbl_OTHER:
		mov ebx,numO			// 
		add [ebx],1				// 
lbl_NEXT:
		dec ecx;				// 
		jne LOOP_X1;			// 

		pop ebx;				// 
		pop ecx;				// 
		pop esi;				// 
		jmp EXIT_C_EXAMPLE;		// let C handle whatever it did upon entering this function

isLetter:
		push ebp;				// 
		mov ebp,esp;			// 
		mov al,[ebp+8];			// 
		cmp al,0x40;			// 
		ja lbl_CHK_ZU;			// check Uppercase 'Z'

lbl_RET_FALSE:
		xor eax,eax;			// 
lbl_RET:
		mov esp,ebp;			// 
		pop ebp;				// 
		ret;					// 

lbl_RET_TRUE:
		mov eax,1;				// 
		jmp lbl_RET;			// 

lbl_CHK_ZU:
		cmp al,0x5B;			// 
		jb lbl_RET_TRUE;		// 

		cmp al,0x61;			// 
		jb lbl_RET_FALSE;		// check lowercase 'z'

		cmp al,0x7A;			// 
		jbe lbl_RET_TRUE;		// 
		jmp lbl_RET_FALSE;		// 

	} // end assembly block

EXIT_C_EXAMPLE:					// 
	return;
} // exCountLetters




//////////////////////////////////////////////////////////////////////////////////////////////////
// code to encrypt the data as specified by the project assignment
void encryptData(char *data, int filesize) //had to change param name since it apparently the inline assembler does not like param names called length
{
	// you can not declare any local variables in C, set up the stack frame and 
	// assign them in assembly
	__asm {

		//clear out all the regs that we are going to use.
		//in place of local variables, these registers are used.
		mov edx, 0;
		mov ecx, 0;
		mov eax, 0;
		mov ebx, 0;
		mov esi, 0;
		mov edi, 0;
		jmp OuterLoop;


	OuterLoop:    //this is the initial for loop, 
		cmp ecx, gNumRounds; //comparing the rounds, and if we have done enough iterations, leave
		jge EXIT_C_ENCRYPT_DATA;


		mov esi, gptrPasswordHash; //gotta be able to access that array data, right?


		//ax is the index,
		//bx is the hopcount
		
		//to put the data in the register correctly, 
		//add it to the reg, do an arithmetic shift left by 8 (which is the equivalent of mul by 256),
		//then add it again.
		add al, [esi + 0 + ecx * 4];
		sal eax, 8;
		add al, [esi + 1 + ecx * 4];

		add bl, [esi + 2 + ecx * 4];
		sal ebx, 8;
		add bl, [esi + 3 + ecx * 4];

		mov esi, 0; //reset esi since we are gonna use that later
		


		inc ecx; //increment the counter, but then push it to save its state
		push ecx; //since it needs to be used in the inner loop
		mov ecx, 0; //new loop value
		cmp bx, 0; //need to check if hopcount is zero and if so, set it to 0xFFFFh
		je SET_BX;
		jmp INNER_LOOP;

	SET_BX:
		mov bx, 0xFFFF;
		jmp INNER_LOOP;



	INNER_LOOP:
		cmp ecx, filesize; //check against the filesize, and if its greater or equal to,
		jge OUTERLOOP_MIDPOINT; //break out of the inner loop and go back to the outer loop
		mov edx, eax; //need to use eax's value as an index but also need to use it as a temporary variable
		mov edi, data;
		mov esi, gptrKey;
		push ebx; //save the index and hop count states
		push eax;
		mov eax, 0; //clear the regs (probably not necessary)
		mov ebx, 0;
		mov bl, [edi + ecx]; //take a byte from the data buffer
		mov al, [esi + edx]; //and the key file
		PXOR bl, al; //no clue why the normal xor instruction does not work here, but PXOR does accomplish what is needed
		mov [edi + ecx], bl; //then put that byte back in the data buffer
		pop eax; //restore the states!
		pop ebx;
		add eax, ebx;
		mov edx, 0; //reset edx
		cmp eax, 0x10001; //if the index is greater 65537, subtract that amount 
		jge INC_HOP_COUNT; //and begin working on the actual bytes
		jmp CRYPTO_PART;

	INC_HOP_COUNT:
		sub eax, 0x10001;
		jmp CRYPTO_PART;

	OUTERLOOP_MIDPOINT: //used as a way to restore the counter of the outer loop
		pop ecx;
		jmp OuterLoop;

	CRYPTO_PART:
		push ebx; //gotta save that hop count
		mov ebx, 0;
		mov esi, ecx; //im pretty sure this is actually redundant, but use esi as the index
		mov bl, [edi + esi]; //Begin the encryption process!
		ror bl, 1; //rotate the byte right by one and then put it back in the buffer
		mov [edi + esi], bl;

		
		/***********swap the nibbles ***************/
		mov dl, 0;
		mov dl, bl;
		rol dl, 4;
		/*******************************************/


		mov[edi + esi], dl; //then moved the operated byte back into the data buffer
		mov bl, [edi + esi]; //and begin again


		/**********Reverse the bits*********/
		mov dl, 0; 
		push ecx;
		mov cl, bl;
		and bl, 0xF0;
		shr bl, 4;
		and cl, 0x0F;
		shl cl, 4;
		or bl, cl; // (var & 0xF0) >> 4 | (var & 0x0F) << 4;
		mov dl, bl;
		mov cl, bl;
		and bl, 0xCC;
		shr bl, 2;
		and cl, 0x33;
		shl cl, 2;
		or bl, cl;
		mov dl, bl; //(var & 0xCC) >> 2 | (var & 0x33) << 2;
		mov cl, bl;
		and bl, 0xAA;
		shr bl, 1;
		and cl, 0x55;
		shl cl, 1;
		or bl, cl;
		mov dl, bl; //(var & 0xAA) >> 1 | (var & 0x55) << 1;
		pop ecx; //restor the counter
		/***********************************/

		mov[edi + esi], dl;
		mov bl, [edi + esi];

		/************************swap the half nibbles****************************/
		mov dl, bl;
		and dl, 0xCC;
		and bl, 0x33;
		shr dl, 2;
		shl bl, 2;
		or dl, bl;
		/*****************************************************/
		mov[edi + esi], dl;
		mov bl, [edi + esi];
		
		//do the final rotation, then put the byte back into the array, 
		//restore the hop count, increment the counter, 
		//and begin the loop again.
		rol bl, 1;
		mov[edi + esi], bl;
		pop ebx;
		add ecx, 1;
		jmp INNER_LOOP;

	}

EXIT_C_ENCRYPT_DATA:
	return;
} // encryptData

// code to read the file to encrypt
int encryptFile(FILE *fptrIn, FILE *fptrOut)
{
	char *buffer;
	unsigned int filesize;

	filesize = _filelength(_fileno(fptrIn));	// Linux???
	if(filesize > 0x1000000)					// 16 MB, file too large
	{
		fprintf(stderr, "Error - Input file too large.\n\n");
		return -1;
	}

	// use the password hash to encrypt
	buffer = (char *) malloc(filesize);
	if(buffer == NULL)
	{
		fprintf(stderr, "Error - Could not allocate %d bytes of memory on the heap.\n\n", filesize);
		return -1;
	}

	fread(buffer, 1, filesize, fptrIn);	// read entire file
	encryptData(buffer, filesize);
	fwrite(buffer, 1, filesize, fptrOut);
	free(buffer);

	return 0;
} // encryptFile


//////////////////////////////////////////////////////////////////////////////////////////////////
// code to decrypt the data as specified by the project assignment
void decryptData(char *data, int filesize)
{
	// you can not declare any local variables in C, set up the stack frame and 
	// assign them in assembly
	__asm {

		//clear out all the regs that we are going to use.
		//in place of local variables, these registers are used.
		mov edx, 0;
		mov ecx, 0;
		mov eax, 0;
		mov ebx, 0;
		mov esi, 0;
		mov edi, 0;
		jmp OuterLoop;

		//The code for the decryption is essentially the same as the code for the encryption,
		//the only difference being that we rotate to the left first,
		//swap the half nibbles, reverse the bits, swap the nibbles,
		//and rotate to right
		//which reverses the entire encryption process

	OuterLoop:
		cmp ecx, gNumRounds;
		jge EXIT_C_DECRYPT_DATA;


		mov esi, gptrPasswordHash;

		//ax is the index,
		//bx is the hopcount

		//to put the data in the register correctly, 
		//add it to the reg, do an arithmetic shift left by 8 (which is the equivalent of mul by 256),
		//then add it again.

		add al, [esi + 0 + ecx * 4];
		sal eax, 8;
		add al, [esi + 1 + ecx * 4];

		add bl, [esi + 2 + ecx * 4];
		sal ebx, 8;
		add bl, [esi + 3 + ecx * 4];

		mov esi, 0;



		inc ecx;
		push ecx;
		mov ecx, 0;
		cmp bx, 0; //need to chck if hopcount is zero and if so, set it to 0xFFFFh
		je SET_BX;
		jmp INNER_LOOP;

	SET_BX:
		mov bx, 0xFFFF;
		jmp INNER_LOOP;


	INNER_LOOP:
		cmp ecx, filesize;
		jge OUTERLOOP_MIDPOINT;
		mov edx, eax;
		mov edi, data;
		mov esi, gptrKey;
		push ebx;
		push eax;
		mov eax, 0;
		mov ebx, 0;
		mov bl, [edi + ecx];
		mov al, [esi + edx];
		PXOR bl, al; //No freaking clue why the normal XOR does not work here, but PXOR does
		mov[edi + ecx], bl;
		pop eax;
		pop ebx;

		add eax, ebx;
		mov edx, 0;
		cmp eax, 0x10001;
		jge INC_HOP_COUNT;
		jmp CRYPTO_PART;

	INC_HOP_COUNT:
		sub eax, 0x10001;
		jmp CRYPTO_PART;

	OUTERLOOP_MIDPOINT:
		pop ecx;
		jmp OuterLoop;

	CRYPTO_PART:
		push ebx;
		mov ebx, 0;
		mov esi, ecx;
		mov bl, [edi + esi];
		ror bl, 1;
		mov[edi + esi], bl;


		/************************swap the half nibbles****************************/
		mov dl, bl;
		and dl, 0xCC;
		and bl, 0x33;
		shr dl, 2;
		shl bl, 2;
		or dl, bl;
		/*****************************************************/


		mov[edi + esi], dl;
		mov bl, [edi + esi];


		/**********Reverse the bits*********/
		mov dl, 0; //clear edx, as its our ret value
		push ecx;
		mov cl, bl;
		and bl, 0xF0;
		shr bl, 4;
		and cl, 0x0F;
		shl cl, 4;
		or bl, cl; //(var & 0xF0) >> 4 | (var & 0x0F) << 4;
		mov dl, bl;
		mov cl, bl;
		and bl, 0xCC;
		shr bl, 2;
		and cl, 0x33;
		shl cl, 2;
		or bl, cl;
		mov dl, bl; //(var & 0xCC) >> 2 | (var & 0x33) << 2;
		mov cl, bl;
		and bl, 0xAA;
		shr bl, 1;
		and cl, 0x55;
		shl cl, 1;
		or bl, cl;
		mov dl, bl; //(b & 0xAA) >> 1 | (b & 0x55) << 1;
		pop ecx;
		/**********************/

		mov[edi + esi], dl;
		mov bl, [edi + esi];

		/***********swap the nibbles ***************/
		mov dl, 0;
		mov dl, bl;
		rol dl, 4;
		/*******************************************/

		mov[edi + esi], dl;
		mov bl, [edi + esi];



		rol bl, 1;
		mov[edi + esi], bl;
		pop ebx;
		add ecx, 1;
		jmp INNER_LOOP;

	}

EXIT_C_DECRYPT_DATA:
	return;
} // decryptData

// code to read in file and prepare for decryption
int decryptFile(FILE *fptrIn, FILE *fptrOut)
{
	char *buffer;
	unsigned int filesize;

	filesize = _filelength(_fileno(fptrIn));	// Linux???
	if(filesize > 0x1000000)					// 16 MB, file too large
	{
		fprintf(stderr, "Error - Input file too large.\n\n");
		return -1;
	}

	// use the password hash to encrypt
	buffer = (char *) malloc(filesize);
	if(buffer == NULL)
	{
		fprintf(stderr, "Error - Could not allocate %d bytes of memory on the heap.\n\n", filesize);
		return -1;
	}

	fread(buffer, 1, filesize, fptrIn);	// read entire file
	decryptData(buffer, filesize);
	fwrite(buffer, 1, filesize, fptrOut);
	free(buffer);

	return 0;
} // decryptFile


//////////////////////////////////////////////////////////////////////////////////////////////////
FILE *openInputFile(char *filename)
{
	FILE *fptr;

	fptr = fopen(filename, "rb");
	if(fptr == NULL)
	{
		fprintf(stderr, "\n\nError - Could not open input file %s!\n\n", filename);
		exit(-1);
	}
	return fptr;
} // openInputFile

FILE *openOutputFile(char *filename)
{
	FILE *fptr;

	fptr = fopen(filename, "wb+");
	if(fptr == NULL)
	{
		fprintf(stderr, "\n\nError - Could not open output file %s!\n\n", filename);
		exit(-1);
	}
	return fptr;
} // openOutputFile


void usage(char *argv[])	//   cryptor.exe -e -i <input file> �k <keyfile> -p <password> [�r <#rounds>]
{
	printf("\n\nUsage:\n\n");
	printf("%s -<e=encrypt or d=decrypt> -i <message_filename> -k <keyfile> -p <password> [-r <#rounds>]\n\n", argv[0]);
	printf("-e				:encrypt the specified file\n");
	printf("-d				:decrypt the specified file\n");
	printf("-i filename		:the name of the file to encrypt or decrypt\n");
	printf("-p password		:the password to be used for encryption [default='password']\n");
	printf("-r <#rounds>	:number of encryption rounds (1 - 3)  [default = 1]\n");
	printf("-o filename		:name of the output file [default='encrypted.txt' or 'decrypted.txt'\n\n");
	exit(0);
} // usage

void parseCommandLine(int argc, char *argv[])
{
	int cnt;
	char ch;
	bool i_flag, o_flag, k_flag, p_flag, err_flag;

	i_flag = k_flag = false;				// these must be true in order to exit this function
	err_flag = p_flag = o_flag = false;		// these will generate different actions

	cnt = 1;	// skip program name
	while(cnt < argc)
	{
		ch = *argv[cnt];
		if(ch != '-')
		{
			fprintf(stderr, "All options must be preceeded by a dash '-'\n\n");
			usage(argv);
		}

		ch = *(argv[cnt]+1);
		if(0)
		{
		}

		else if(ch == 'e' || ch == 'E')
		{
			if(gOp != 0)
			{
				fprintf(stderr, "Error! Already specified encrypt or decrypt.\n\n");
				usage(argv);
			}
			gOp = 1;	// encrypt
		}

		else if(ch == 'd' || ch == 'D')
		{
			if(gOp != 0)
			{
				fprintf(stderr, "Error! Already specified encrypt or decrypt.\n\n");
				usage(argv);
			}
			gOp = 2;	// decrypt
		}

		else if(ch == 'i' || ch == 'I')
		{
			if(i_flag == true)
			{
				fprintf(stderr, "Error! Already specifed an input file.\n\n");
				usage(argv);
			}
			i_flag = true;
			cnt++;
			if(cnt >= argc)
			{
				fprintf(stderr, "Error! Must specify a filename after '-i'\n\n");
				usage(argv);
			}
			strncpy(gInFileName, argv[cnt], 256);
		}

		else if(ch == 'o' || ch == 'O')
		{
			if(o_flag == true)
			{
				fprintf(stderr, "Error! Already specifed an output file.\n\n");
				usage(argv);
			}
			o_flag = true;
			cnt++;
			if(cnt >= argc)
			{
				fprintf(stderr, "Error! Must specify a filename after '-o'\n\n");
				usage(argv);
			}
			strncpy(gOutFileName, argv[cnt], 256);
		}

		else if(ch == 'k' || ch == 'K')
		{
			if(k_flag == true)
			{
				fprintf(stderr, "Error! Already specifed a key file.\n\n");
				usage(argv);
			}
			k_flag = true;
			cnt++;
			if(cnt >= argc)
			{
				fprintf(stderr, "Error! Must specify a filename after '-k'\n\n");
				usage(argv);
			}
			strncpy(gKeyFileName, argv[cnt], 256);
		}

		else if(ch == 'p' || ch == 'P')
		{
			if(p_flag == true)
			{
				fprintf(stderr, "Error! Already specifed a password.\n\n");
				usage(argv);
			}
			p_flag = true;
			cnt++;
			if(cnt >= argc)
			{
				fprintf(stderr, "Error! Must enter a password after '-p'\n\n");
				usage(argv);
			}
			strncpy(gPassword, argv[cnt], 256);
		}

		else if(ch == 'r' || ch == 'R')
		{
			int x;

			cnt++;
			if(cnt >= argc)
			{
				fprintf(stderr, "Error! Must enter number between 1 and 3 after '-r'\n\n");
				usage(argv);
			}
			x = atoi(argv[cnt]);
			if(x < 1 || x > 3)
			{
				fprintf(stderr, "Warning! Entered bad value for number of rounds. Setting it to one.\n\n");
				x = 1;
			}
			gNumRounds = x;
		}

		else
		{
			fprintf(stderr, "Error! Illegal option in argument. %s\n\n", argv[cnt]);
			usage(argv);
		}

		cnt++;
	} // end while

	if(gOp == 0)
	{
		fprintf(stderr, "Error! Encrypt or Decrypt must be specified.\n\n)");
		err_flag = true;
	}

	if(i_flag == false)
	{
		fprintf(stderr, "Error! No input file specified.\n\n");
		err_flag = true;
	}

	if(k_flag == false)
	{
		fprintf(stderr, "Error! No key file specified.\n\n");
		err_flag = true;
	}

	if(p_flag == false)
	{
		fprintf(stderr, "Warning! Using default 'password'.\n\n");
	}

	if(o_flag == false && err_flag == false)	// no need to do this if we have errors
	{
		strcpy(gOutFileName, gInFileName);
		if(gOp == 1)	// encrypt
		{
			strcat(gOutFileName, ".enc");
		}
		if(gOp == 2)	// decrypt
		{
			strcat(gOutFileName, ".dec");
		}
	}

	if(err_flag)
	{
		usage(argv);
	}
	return;
} // parseCommandLine


void main(int argc, char *argv[])
{
#ifdef TEST_CODE
	char testData[] = "The big lazy brown FOX jumped 123 the 987 dog. Then he 8 a CHICKEN.";
	int numCAPS, numLow, numNonLetters;
	numCAPS = numLow = numNonLetters = 0;
	exCountLetters(testData, strlen(testData), &numCAPS, &numLow, &numNonLetters);
	printf("numCAPS=%d, numLow=%d, numNonLetters=%d\n", numCAPS, numLow, numNonLetters );
	exit(0);
#endif

	int length, resulti;

	// parse command line parameters
	parseCommandLine(argc, argv);		// sets global variables, checks input options for errors

	// open the input and output files
	gfptrIn = openInputFile(gInFileName);
	gfptrKey = openInputFile(gKeyFileName);
	gfptrOut = openOutputFile(gOutFileName);

	length = (size_t) strlen(gPassword);

	resulti = sha256(NULL, gPassword, length, gPasswordHash);		// get sha-256 hash of password
	if(resulti != 0)
	{
		fprintf(stderr, "Error! Password not hashed correctly.\n\n");
		exit(-1);
	}

	length = fread(gkey, 1, 65537, gfptrKey);
	if(length != 65537)
	{
		fprintf(stderr, "Error! Length of key file is not at least 65537.\n\n");
		exit(-1);
	}
	fclose(gfptrKey);
	gfptrKey = NULL;

	if(gOp == 1)	// encrypt
	{
		encryptFile(gfptrIn, gfptrOut);
	}
	else
	{
		decryptFile(gfptrIn, gfptrOut);
	}

	fclose(gfptrIn);
	fclose(gfptrOut);
	return;
} // main