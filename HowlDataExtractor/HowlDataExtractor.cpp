// NDI HOWL Extractor WIP!
// by Xanvier / xan1242
// Beware of bad code ahead!
// TODO: add sample pack extraction, file conversion, code restructuring, etc.

#include "stdafx.h"
#include <stdlib.h>
#include <string.h>

struct DataOffsetStruct
{
	unsigned int Unk1;
	unsigned int Unk2;
	unsigned int SampleBlockTable;
	unsigned int SequencePackTable;
	unsigned int Unk3;
}DataOffsets, DataSizes;

char ExecBuf[255];

void ExportParsedDataToTxtFile(const char* FileName)
{
	char *OutTxtFileName = (char*)malloc(strlen(FileName)+2);
	strcpy(OutTxtFileName, FileName);
	OutTxtFileName[strlen(OutTxtFileName) - 1] = 't';
	OutTxtFileName[strlen(OutTxtFileName) - 2] = 'x';
	OutTxtFileName[strlen(OutTxtFileName) - 3] = 't';

	printf("Writing offset and size data to %s\n", OutTxtFileName);

	FILE *foutput = fopen(OutTxtFileName, "w");

	if (foutput == NULL)
	{
		perror("ERROR");
		free(OutTxtFileName);
		return;
	}

	fprintf(foutput, "Unk1: offset = %X size = %X\n", DataOffsets.Unk1, DataSizes.Unk1);
	fprintf(foutput, "Unk2: offset = %X size = %X\n", DataOffsets.Unk2, DataSizes.Unk2);
	fprintf(foutput, "SampleBlockTable: offset = %X size = %X\n", DataOffsets.SampleBlockTable, DataSizes.SampleBlockTable);
	fprintf(foutput, "SequencePackTable: offset = %X size = %X\n", DataOffsets.SequencePackTable, DataSizes.SequencePackTable);
	fprintf(foutput, "Unk3: offset = %X", DataOffsets.Unk3);

	fclose(foutput);

	free(OutTxtFileName);
}

bool DoBufferedFileCopying(FILE *finput, unsigned int offset, unsigned int length, const char* OutputFileName)
{
	void *FileBuf;
	FileBuf = malloc(length);
	fseek(finput, offset, SEEK_SET);
	fread(FileBuf, length, 1, finput);

	FILE *foutput = fopen(OutputFileName, "wb");
	if (foutput == NULL)
	{
		perror("ERROR");
		return 0;
	}

	fwrite(FileBuf, length, 1, foutput);

	free(FileBuf);
	return 1;
}

void ExportData(FILE *finput, const char* FileName)
{
	char *OutFileNameBase = (char*)malloc(strlen(FileName)+2);
	strcpy(OutFileNameBase, FileName);
	OutFileNameBase[4] = '\0';
	char *OutFileName = (char*)malloc(strlen(FileName)+19);
	// Unk1
	strcpy(OutFileName, OutFileNameBase);
	strcat(OutFileName, "_Unk1.bin");
	printf("Writing Unk1 data to %s\n", OutFileName);
	if (!DoBufferedFileCopying(finput, DataOffsets.Unk1, DataSizes.Unk1, OutFileName))
		printf("Error writing Unk1 data.\n");
	// Unk2
	strcpy(OutFileName, OutFileNameBase);
	strcat(OutFileName, "_Unk2.bin");
	printf("Writing Unk2 data to %s\n", OutFileName);
	if (!DoBufferedFileCopying(finput, DataOffsets.Unk2, DataSizes.Unk2, OutFileName))
		printf("Error writing Unk2 data.\n");
	// SampleBlockTable
	strcpy(OutFileName, OutFileNameBase);
	strcat(OutFileName, "_SampleBlockTable.bin");
	printf("Writing SampleBlockTable data to %s\n", OutFileName);
	if (!DoBufferedFileCopying(finput, DataOffsets.SampleBlockTable, DataSizes.SampleBlockTable, OutFileName))
		printf("Error writing SampleBlockTable data.\n");
	// SequencePackTable
	strcpy(OutFileName, OutFileNameBase);
	strcat(OutFileName, "_SequencePackTable.bin");
	printf("Writing SequencePackTable data to %s\n", OutFileName);
	if (!DoBufferedFileCopying(finput, DataOffsets.SequencePackTable, DataSizes.SequencePackTable, OutFileName))
		printf("Error writing SequencePackTable data.\n");

	free(OutFileName);
	free(OutFileNameBase);
}

void ExtractDataFromTables(FILE *finput, const char* FileName)
{
	struct stat s;
	fstat(_fileno(finput), &s);
	FILE* OutSequenceFile;

	char *OutFileNameBase = (char*)malloc(strlen(FileName)+2);
	strcpy(OutFileNameBase, FileName);
	OutFileNameBase[4] = '\0';
	char *OutFileName = (char*)malloc(strlen(FileName) + 19);

	unsigned int SequenceCount = DataSizes.SequencePackTable / sizeof(short int);
	unsigned int* ExtractOffsetList = (unsigned int*)calloc(SequenceCount, sizeof(unsigned int));
	unsigned int* ExtractSizeList = (unsigned int*)calloc(SequenceCount, sizeof(unsigned int));

	fseek(finput, DataOffsets.SequencePackTable, SEEK_SET);
	for (unsigned int i = 0; i < SequenceCount; i++)
	{
		fread(&ExtractOffsetList[i], sizeof(short int), 1, finput);
		ExtractOffsetList[i] <<= 11;
	}

	for (unsigned int i = 0; i < SequenceCount; i++)
	{
		if ((i + 1) < SequenceCount)
			ExtractSizeList[i] = ExtractOffsetList[i + 1] - ExtractOffsetList[i];
		else
			ExtractSizeList[i] = s.st_size - ExtractOffsetList[i];
	}

	sprintf(ExecBuf, "md %s_SequencePacks", OutFileNameBase);
	system(ExecBuf);

	for (unsigned int i = 0; i < SequenceCount; i++)
	{
		sprintf(OutFileName, "%s_SequencePacks\\%d.seq", OutFileNameBase, i);
		if (!DoBufferedFileCopying(finput, ExtractOffsetList[i], ExtractSizeList[i], OutFileName))
			printf("Error writing %s\n", OutFileName);
	}
	
	free(OutFileName);
	free(OutFileNameBase);
}

void ParseData(FILE *finput)
{
	struct stat s;
	fstat(_fileno(finput), &s);

	fseek(finput, 0x10, SEEK_SET);
	fread(&DataOffsets.Unk1, 2, 1, finput);
	fseek(finput, 0x14, SEEK_SET);
	fread(&DataOffsets.Unk2, 2, 1, finput);
	fseek(finput, 0x18, SEEK_SET);
	fread(&DataOffsets.SampleBlockTable, 2, 1, finput);
	fseek(finput, 0x1C, SEEK_SET);
	fread(&DataOffsets.SequencePackTable, 2, 1, finput);
	fseek(finput, 0x20, SEEK_SET);
	fread(&DataOffsets.Unk3, 2, 1, finput);

	DataOffsets.Unk1 = (DataOffsets.Unk1 << 2) + 0x28;
	DataOffsets.Unk2 = (DataOffsets.Unk2 << 3) + DataOffsets.Unk1;
	DataOffsets.SampleBlockTable = (DataOffsets.SampleBlockTable << 3) + DataOffsets.Unk2;
	DataOffsets.SequencePackTable = (DataOffsets.SequencePackTable << 1) + DataOffsets.SampleBlockTable;
	DataOffsets.Unk3 = (DataOffsets.Unk3 << 1) + DataOffsets.SequencePackTable;
	

	DataSizes.Unk1 = DataOffsets.Unk2 - DataOffsets.Unk1;
	DataSizes.Unk2 = DataOffsets.SampleBlockTable - DataOffsets.Unk2;
	DataSizes.SampleBlockTable = DataOffsets.SequencePackTable - DataOffsets.SampleBlockTable;
	DataSizes.SequencePackTable = DataOffsets.Unk3 - DataOffsets.SequencePackTable;

	printf("Unk1 offset: %X size: %X\n", DataOffsets.Unk1, DataSizes.Unk1);
	printf("Unk2 offset: %X size: %X\n", DataOffsets.Unk2, DataSizes.Unk2);
	printf("SampleBlockTable offset: %X size: %X\n", DataOffsets.SampleBlockTable, DataSizes.SampleBlockTable);
	printf("SequencePackTable offset: %X size: %X\n", DataOffsets.SequencePackTable, DataSizes.SequencePackTable);
	printf("Unk3 offset: %X\n", DataOffsets.Unk3);
}

int main(int argc, char *argv[])
{
	printf("NDI HOWL extractor\n");
	if (argc < 2)
	{
		printf("ERROR: Too few arguments.\nUSAGE: %s KART.HWL\n", argv[0]);
		return -1;
	}
	printf("Opening %s\n", argv[1]);
	FILE *fin = fopen(argv[1], "rb");
	if (fin == NULL)
	{
		perror("ERROR");
		return -1;
	}
	printf("Parsing data\n");
	ParseData(fin);
	printf("Exporting parsed data to text\n");
	ExportParsedDataToTxtFile(argv[1]);
	printf("Exporting data to separate files\n");
	ExportData(fin, argv[1]);
	printf("Extracting data from offset tables\n");
	ExtractDataFromTables(fin, argv[1]);
	fclose(fin);
    return 0;
}

