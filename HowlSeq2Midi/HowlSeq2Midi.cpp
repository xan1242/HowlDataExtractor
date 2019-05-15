// HOWL Seq2Midi
// Converts HOWL SequencePacks to MIDI!
// by Xanvier / xan1242

#include "stdafx.h"
#include <stdlib.h>
#include <string.h>

#define MIDI_HEADER 0x6468544D // "MThd" in little endian
#define MIDI_TRACK_HEADER 0x6B72544D // "MTrk" in little endian

struct MainDataBlock
{
	short int TotalFileSize;
	short int unk2;
	unsigned char InstrumentCount; // each size 0xC
	unsigned char DrumInstrumentCount; // each size 0x8
	short int NumberOfSequences;
};

struct InstrumentInfoDataBlock
{
	unsigned char unk1;
	unsigned char volume;
	short int unk2;
	short int pitchthing; // related to pitch calculations, unknown as of yet, high byte can change semitones, low byte fine pitch?
	short int InstrumentSampleOffset; // instrument sample offset? changing the high byte to 0 causes countdown horn
	short int ADSR; // ADSR, high byte attack, low byte release, signed, somewhere in between is decay and sustain
	short int unk6; // unknown, really unknown
};

struct DrumTrackInfoDataBlock
{
	unsigned char unk1;
	unsigned char volume;
	short int pitchthing;
	short int unk2;
	short int unk3;
};

struct MultiTrackDataBlock // used to point to invincibility music or something idk.
{
	short int unk1; // offsets tempo info?
	short int SeqTrackOffset;
	short int SeqTrackOffset2;
	short int unk4;
};

struct TempoBlock
{
	unsigned char bitshifter; // to left
	unsigned char TrackCount;
	short int TempoBPM;
	short int PPQN; // or TPQN
//	short int unk2;
};

struct HowlSequencePack
{
	MainDataBlock mdb;
	InstrumentInfoDataBlock *iidb;
	DrumTrackInfoDataBlock *dtidb; // maybe not just for drums, but for samples in general
	MultiTrackDataBlock mtdb; // invincibility music...
	TempoBlock tb;
	short int* TrackOffsets; // maybe wrong... relative offsets to the beginning of the data (sequencedata here)
	void* sequencedata;
}SeqPack;

unsigned int SeqPackDataSize;
unsigned int MainTBLocation;
char TrackName[64];
char MultiSeqName[255];
char OutFileName[255];

unsigned int VerbosityLevel = 1;

// my code from NFS projects...
int __stdcall XNFS_printf(unsigned int Level, const char* Format, ...)
{
	va_list ArgList;
	int Result = 0;
	if ((Level <= VerbosityLevel) && VerbosityLevel)
	{
		__crt_va_start(ArgList, Format);
		Result = vfprintf(stdout, Format, ArgList);
		__crt_va_end(ArgList);
	}
	return Result;
}

// moves the offset cursor to the actual track start because the format sometimes likes to start tracks with 2 bytes after
void DetectTrackStart(FILE* finput)
{
	unsigned int OldOffset = ftell(finput);
	short int Read1 = 0;
	short int Read2 = 0;

	fread(&Read1, sizeof(short int), 1, finput);
	fread(&Read2, sizeof(short int), 1, finput);

	if (Read1 == 1)
	{
		fseek(finput, OldOffset, SEEK_SET);
		return;
	}
	if (Read1 == 0 && Read2 == 1)
	{
		fseek(finput, -2, SEEK_CUR);
		return;
	}
	fseek(finput, OldOffset, SEEK_SET);
}

void ParseMainSeqData(FILE* finput)
{
	fread(&SeqPack.mdb, sizeof(MainDataBlock), 1, finput);
	// allocate memory...
	SeqPack.iidb = (InstrumentInfoDataBlock*)calloc(SeqPack.mdb.InstrumentCount, sizeof(InstrumentInfoDataBlock));
	SeqPack.dtidb = (DrumTrackInfoDataBlock*)calloc(SeqPack.mdb.DrumInstrumentCount, sizeof(DrumTrackInfoDataBlock));
	SeqPack.TrackOffsets = (short int*)calloc(32, sizeof(short int));

	SeqPackDataSize = SeqPack.mdb.TotalFileSize - ((sizeof(InstrumentInfoDataBlock) * SeqPack.mdb.InstrumentCount) + (sizeof(DrumTrackInfoDataBlock) * SeqPack.mdb.DrumInstrumentCount) + (sizeof(short int) * 32));
	XNFS_printf(2, "SeqPack data size: %X\n", SeqPackDataSize);
	SeqPack.sequencedata = malloc(SeqPack.mdb.TotalFileSize);

	// read the seqpack main data
	fread(SeqPack.iidb, sizeof(InstrumentInfoDataBlock), SeqPack.mdb.InstrumentCount, finput);
	fread(SeqPack.dtidb, sizeof(DrumTrackInfoDataBlock), SeqPack.mdb.DrumInstrumentCount, finput);
	if (SeqPack.mdb.NumberOfSequences > 2)
		fread(&SeqPack.mtdb, sizeof(MultiTrackDataBlock), 1, finput);
	else
		fseek(finput, 4, SEEK_CUR);

	MainTBLocation = ftell(finput);
	fread(&SeqPack.tb, sizeof(TempoBlock), 1, finput);

	fread(SeqPack.TrackOffsets, sizeof(short int), SeqPack.tb.TrackCount, finput);
	DetectTrackStart(finput);
	//fseek(finput, 2, SEEK_CUR);
	fread(SeqPack.sequencedata, SeqPack.mdb.TotalFileSize, 1, finput);
}

void ParseNewTrack(FILE* finput, unsigned int RelativeOffset, unsigned int MainTBOffset) // TODO: add reading from memory (SeqPack.sequencedata)
{
	// reparse data for the other songs in a sequence pack
	// it starts from the TempoBlock
	fseek(finput, RelativeOffset + MainTBOffset, SEEK_SET);
	fread(&SeqPack.tb, sizeof(TempoBlock), 1, finput);
	fread(SeqPack.TrackOffsets, sizeof(short int), SeqPack.tb.TrackCount, finput);
	DetectTrackStart(finput);
	fread(SeqPack.sequencedata, SeqPackDataSize, 1, finput);
}

void PrintInfo()
{
	XNFS_printf(2, "HOWL Sequence info:\n\n=Main Data Block:=\n");
	XNFS_printf(2, "Total size: 0x%hX\n", SeqPack.mdb.TotalFileSize);
	XNFS_printf(2, "Instrument count: %d\n", (int)SeqPack.mdb.InstrumentCount);
	XNFS_printf(2, "Drum instrument count: %d\n", (int)SeqPack.mdb.DrumInstrumentCount);
	XNFS_printf(2, "Number of sequences: %d\n", SeqPack.mdb.NumberOfSequences);
	XNFS_printf(2, "Unk1: 0x%hX\n", SeqPack.mdb.unk2);

	XNFS_printf(2, "\n=Instrument info:=\n");
	for (unsigned int i = 0; i < SeqPack.mdb.InstrumentCount; i++)
	{
		XNFS_printf(2, "INSTRUMENT: %d\n", i);
		XNFS_printf(2, "Volume: %d\n", (int)SeqPack.iidb[i].volume);
		XNFS_printf(2, "PitchThing: 0x%hX\n", SeqPack.iidb[i].pitchthing);
		XNFS_printf(2, "InstrumentSampleOffset: 0x%hX\n", SeqPack.iidb[i].InstrumentSampleOffset);
		XNFS_printf(2, "ADSR: 0x%hX\n", SeqPack.iidb[i].ADSR);
		XNFS_printf(2, "Unk1: 0x%X\n", (int)SeqPack.iidb[i].unk1);
		XNFS_printf(2, "Unk2: 0x%X\n", (int)SeqPack.iidb[i].unk2);
		XNFS_printf(2, "Unk3: 0x%X\n\n", (int)SeqPack.iidb[i].unk6);
	}

	XNFS_printf(2, "\n=Drum instrument info:=\n");
	for (unsigned int i = 0; i < SeqPack.mdb.DrumInstrumentCount; i++)
	{
		XNFS_printf(2,"DRUM INSTRUMENT: %d\n", i);
		XNFS_printf(2,"Volume: %d\n", (int)SeqPack.dtidb[i].volume);
		XNFS_printf(2,"PitchThing: 0x%hhX\n", SeqPack.dtidb[i].pitchthing);
		XNFS_printf(2,"Unk1: 0x%X\n", (int)SeqPack.dtidb[i].unk1);
		XNFS_printf(2,"Unk2: 0x%X\n", (int)SeqPack.dtidb[i].unk2);
		XNFS_printf(2,"Unk3: 0x%X\n\n", (int)SeqPack.dtidb[i].unk3);
	}
	XNFS_printf(2,"\n=MultiTrack info (invincibility music):=\n");
	XNFS_printf(2,"InvincibilitySeqTrackOffset (Aku): 0x%X\n", (int)SeqPack.mtdb.SeqTrackOffset);
	XNFS_printf(2,"InvincibilitySeqTrackOffset (Uka): 0x%X\n", (int)SeqPack.mtdb.SeqTrackOffset2);
	XNFS_printf(2,"Unk1: 0x%X\n", (int)SeqPack.mtdb.unk1);
	XNFS_printf(2,"Unk2: 0x%X\n", (int)SeqPack.mtdb.unk4);

	XNFS_printf(2,"\n=TempoBlock info:=\n");
	XNFS_printf(2,"Bitshifter: 0x%X\n", (int)SeqPack.tb.bitshifter);
	XNFS_printf(2,"Tempo (BPM): %d\n", (int)SeqPack.tb.TempoBPM);
	XNFS_printf(2,"PPQN: %d\n", (int)SeqPack.tb.PPQN);
	XNFS_printf(2,"TrackCount: %d\n", (int)SeqPack.tb.TrackCount);
}

void Midi_WriteTrackNameEvent(FILE* foutput, unsigned char DeltaTime, const char* TrackName)
{
	short int value = 0x03FF;
	unsigned char len = strlen(TrackName);
	fputc(DeltaTime, foutput);
	fwrite(&value, sizeof(short int), 1, foutput);
	fwrite(&len, sizeof(char), 1, foutput);
	fwrite(TrackName, 1, strlen(TrackName), foutput);
}

// for 4/4 we use numerator = 4, denominator = 2, clocks = 18, notes = 8
void Midi_WriteTimeSignatureEvent(FILE* foutput, unsigned char DeltaTime, unsigned char Numerator, unsigned char Denominator, unsigned char Clocks, unsigned char NumNotes)
{
	short int value = 0x58FF;
	fputc(DeltaTime, foutput);
	fwrite(&value, sizeof(short int), 1, foutput);
	fputc(0x4, foutput);
	fputc(Numerator, foutput);
	fputc(Denominator, foutput);
	fputc(Clocks, foutput);
	fputc(NumNotes, foutput);
}

void Midi_WriteTempoEvent(FILE* foutput, unsigned char DeltaTime, float TempoBPM)
{
	short int value = 0x51FF;
	float TempoBPms = 60000000 / TempoBPM;
	unsigned int TempoToWrite = (unsigned int)TempoBPms;
	unsigned int TempoToWrite1 = (TempoToWrite & 0xFF0000) >> 16;
	unsigned int TempoToWrite2 = (TempoToWrite & 0xFF00) >> 8;
	unsigned int TempoToWrite3 = (TempoToWrite & 0xFF);
	unsigned char value2;

	fputc(DeltaTime, foutput);
	fwrite(&value, sizeof(short int), 1, foutput);
	fputc(0x3, foutput);

	// three byte big endian sheninigans...
	fputc(TempoToWrite1, foutput);
	fputc(TempoToWrite2, foutput);
	fputc(TempoToWrite3, foutput);
}

void Midi_WriteTrackStart(FILE* foutput, unsigned int *TrackSizeOffset)
{
	unsigned int ChunkHead = MIDI_TRACK_HEADER;
	unsigned int ChunkSize = 0; // we don't know this yet, we'll have to write the location down and update it later...
	fwrite(&ChunkHead, sizeof(int), 1, foutput);
	fwrite(&ChunkSize, sizeof(int), 1, foutput);
	*TrackSizeOffset = ftell(foutput);
}

// here we update the track's size descriptor
void Midi_WriteTrackEnd(FILE* foutput, unsigned char DeltaTime, unsigned int *TrackSizeOffset)
{
	fputc(DeltaTime, foutput);
	fputc(0xFF, foutput);
	fputc(0x2F, foutput);
	fputc(0x00, foutput);
	unsigned long OldOffset = ftell(foutput);
	unsigned long size = ftell(foutput) - *TrackSizeOffset;
	size = _byteswap_ulong(size);
	fseek(foutput, *TrackSizeOffset - 4, SEEK_SET);
	fwrite(&size, sizeof(unsigned long), 1, foutput);
	fseek(foutput, OldOffset, SEEK_SET);
}

void Midi_WriteHeader(FILE* foutput, short int FormatVer, short int TrackCount, short int TPQN)
{
	unsigned int ChunkHead = MIDI_HEADER;
	unsigned int ChunkSize = 0x06000000;
	short int FormatVerWrite = _byteswap_ushort(FormatVer);
	short int TrackCountWrite = _byteswap_ushort(TrackCount);
	short int TPQNWrite = _byteswap_ushort(TPQN);

	fwrite(&ChunkHead, sizeof(int), 1, foutput);
	fwrite(&ChunkSize, sizeof(int), 1, foutput);

	fwrite(&FormatVerWrite, sizeof(short int), 1, foutput);
	fwrite(&TrackCountWrite, sizeof(short int), 1, foutput);
	fwrite(&TPQNWrite, sizeof(short int), 1, foutput);
}

// channels are indexed from 0, delta time VLV up to 32 bits for now
void Midi_WriteNoteOnEvent(FILE* foutput, unsigned int DeltaTime, unsigned int vlv_length, unsigned char Note, unsigned char Channel, unsigned char Velocity)
{
	fwrite(&DeltaTime, vlv_length, 1, foutput);
	fputc(0x90 + Channel, foutput);
	fputc(Note, foutput);
	fputc(Velocity, foutput);
}

void Midi_WriteNoteOffEvent(FILE* foutput, unsigned int DeltaTime, unsigned int vlv_length, unsigned char Note, unsigned char Channel, unsigned char Velocity)
{
	fwrite(&DeltaTime, vlv_length, 1, foutput);
	fputc(0x80 + Channel, foutput);
	fputc(Note, foutput);
	fputc(Velocity, foutput);
}

void Midi_WritePanEvent(FILE* foutput, unsigned int DeltaTime, unsigned int vlv_length, unsigned char Channel, unsigned char Pan)
{
	fwrite(&DeltaTime, vlv_length, 1, foutput);
	fputc(0xB0 + Channel, foutput);
	fputc(0xA, foutput);
	fputc(Pan, foutput);
}

void Midi_WriteVolumeEvent(FILE* foutput, unsigned int DeltaTime, unsigned int vlv_length, unsigned char Channel, unsigned char Volume)
{
	fwrite(&DeltaTime, vlv_length, 1, foutput);
	fputc(0xB0 + Channel, foutput);
	fputc(0x7, foutput);
	fputc(Volume, foutput);
}

void ConvertEvents(FILE* foutput, unsigned char Channel, unsigned int Start)
{
	//unsigned int Cursor = 0x200C; // testing values used with Crash Cove
	unsigned int Cursor = Start/* + 2*/;
	//unsigned int End = Start + Size;
	//unsigned int Start = 0x2004;
	//unsigned int End = 0x2544;
	//unsigned int size = 0x540;
	unsigned int delta = 0;
	unsigned char cmd = 0;
	signed char cmd_param1 = 0;
	unsigned char cmd_param2 = 0;
	unsigned int vlv_length = 1;
	float valpercent = 0;
	bool bTrackEnd = false;

	//printf("Current val: %hX\n", *(short int*)((unsigned int)(SeqPack.sequencedata) + Cursor));

	//while (Cursor < End)
	while (!bTrackEnd)
	{
		if (*(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor) >= 0x80)
		{
			// read next VLV byte (doing this up to 4 times for 32 bits of data), hacky and dirty, I don't care
			if (*(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1) >= 0x80)
			{
				if (*(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 2) >= 0x80)
				{
					delta = *(unsigned int*)((unsigned int)(SeqPack.sequencedata) + Cursor);
					vlv_length = 4;
				}
				else
				{
					delta = *(unsigned int*)((unsigned int)(SeqPack.sequencedata) + Cursor);
					delta &= 0x00FFFFFF; // values are big endian
					vlv_length = 3;
				}
			}
			else
			{
				delta = *(short int*)((unsigned int)(SeqPack.sequencedata) + Cursor);
				//delta = _byteswap_ushort(delta);
				vlv_length = 2;
			}
			
		}
		else
		{
			delta = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor);
			vlv_length = 1;
		}
		Cursor += vlv_length;
		cmd = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor);

		switch (cmd)
		{
		case 0:
			Cursor += 1;
			break;
		case 1:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			XNFS_printf(3, "delta: %X\tnote off: %X\n", delta, cmd_param1);
			Cursor += 2;
			Midi_WriteNoteOffEvent(foutput, delta, vlv_length, cmd_param1, Channel, 0);
			break;
		case 2:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			XNFS_printf(3, "WARNING: delta: %X\tunk cmd (2): %X\n", delta, cmd_param1);
			Cursor += 2;
			break;
		case 3:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			XNFS_printf(3, "delta: %X\ttrack end: %X\n", delta, cmd_param1);
			Cursor += 2;
			bTrackEnd = true;
			return;
			break;
		case 4:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			XNFS_printf(3, "WARNING: delta: %X\tunk cmd (4): %X\n", delta, cmd_param1);
			Cursor += 2;
			break;
		case 5:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			cmd_param2 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 2);
			XNFS_printf(3, "delta: %X\tnote on: %X\tvel: %d\n", delta, cmd_param1, cmd_param2);
			Cursor += 3;
			Midi_WriteNoteOnEvent(foutput, delta, vlv_length, cmd_param1, Channel, cmd_param2);
			break;
		case 6:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			XNFS_printf(3, "delta: %X\ttrack volume: %d\t", delta, cmd_param1);
			// convert value to a percentage then apply it to the 0 - 127 range (truncate to 4 bits using float precision)
			valpercent = (float)(cmd_param1) / 255;
			cmd_param1 = (unsigned char)(valpercent * 127.0);
			XNFS_printf(3, "converting to: %d (%.2f%%)\n", cmd_param1, valpercent*100);
			Midi_WriteVolumeEvent(foutput, delta, vlv_length, Channel, cmd_param1);
			Cursor += 2;
			break;
		case 7:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			cmd_param1 = -cmd_param1; // HOWL swaps L and R around...
			XNFS_printf(3, "delta: %X\ttrack pan: %d\n", delta, cmd_param1); 
			Midi_WritePanEvent(foutput, delta, vlv_length, Channel, cmd_param1);
			Cursor += 2;
			break;
		case 9:
			cmd_param1 = *(unsigned char*)((unsigned int)(SeqPack.sequencedata) + Cursor + 1);
			XNFS_printf(3, "delta: %X\ttrack start: %X\n", delta, cmd_param1);
			if (Channel != 9) // not writing volume for drum tracks yet! TODO: add function to extract drum tracks separately (ignore ch 10 directive)
			{
				// convert value to a percentage then apply it to the 0 - 127 range (truncate to 4 bits using float precision)
				valpercent = (float)(SeqPack.iidb[cmd_param1].volume) / 255;
				cmd_param2 = (unsigned char)(valpercent * 127.0); // stealing cmd_param2 here...
				Midi_WriteVolumeEvent(foutput, 0, 1, Channel, cmd_param2);
			}
			Cursor += 2;
			break;
		default:
			XNFS_printf(1, "sequencedata: %X\n", SeqPack.sequencedata);
			XNFS_printf(1, "PANIC! UNKNOWN COMMAND: %X\nPAUSING HERE!\noffset: %X\n", cmd, Cursor);
			getchar();
			break;
		}

	}

}

void ConvertToMidi(const char* OutFileName)
{
	FILE* foutput = fopen(OutFileName, "wb");
	if (foutput == NULL)
	{
		perror("ERROR");
		return;
	}

	// endianness kicks in...
	unsigned int TrackSizeOffset = 0;
	unsigned int CurMelodicChannel = 0;

	Midi_WriteHeader(foutput, 1, SeqPack.tb.TrackCount + 1, SeqPack.tb.PPQN);

	// start master track for tempo shenanigans
	Midi_WriteTrackStart(foutput, &TrackSizeOffset);
	//Midi_WriteTimeSignatureEvent(foutput, 0, 4, 2, 18, 8); // breaks the tempo if wrong values are used, not very necessary in a CTR midi anyway...
	Midi_WriteTempoEvent(foutput, 0, (float)SeqPack.tb.TempoBPM);
	Midi_WriteTrackEnd(foutput, 0, &TrackSizeOffset);
	// end master track

	// start deciphering HOWL sequence tracks and convert them to MIDI
	// TODO FOR EACH TRACK - TRACK PROPERTIES
	for (unsigned int i = 0; i < SeqPack.tb.TrackCount; i++)
	{
		unsigned int TrackSize = 0;
		unsigned int CurChannel = 0;
		bool bDrumTrack = false;
		if (*(short int*)(SeqPack.TrackOffsets[i] + (unsigned int)SeqPack.sequencedata) == 1)
		{
			CurChannel = 9;
			bDrumTrack = true;
		}
		else
			CurChannel = CurMelodicChannel;
		

		if ((i + 1) == SeqPack.tb.TrackCount)
			TrackSize = SeqPackDataSize - SeqPack.TrackOffsets[i];
		else
			TrackSize = SeqPack.TrackOffsets[i + 1] - SeqPack.TrackOffsets[i];
		Midi_WriteTrackStart(foutput, &TrackSizeOffset);
		sprintf(TrackName, "Track 0x%x", SeqPack.TrackOffsets[i]);
		Midi_WriteTrackNameEvent(foutput, 0, TrackName);

		XNFS_printf(1, "Converting: %s\n", TrackName);
		ConvertEvents(foutput, CurChannel, SeqPack.TrackOffsets[i]);
		Midi_WriteTrackEnd(foutput, 0, &TrackSizeOffset);
		if (bDrumTrack)
		{
			bDrumTrack = false;
		}
		else
		{
			CurMelodicChannel++;
			if (CurMelodicChannel == 9)
				CurMelodicChannel++;
			CurMelodicChannel %= 16;
		}

	}

	fclose(foutput);

}

int main(int argc, char *argv[])
{
	printf("NDI HOWL sequence pack converter\n");
	if (argc < 2)
	{
		printf("ERROR: Too few arguments.\nUSAGE: %s HowlSeqFile [-v/-vv]\n", argv[0]);
		return -1;
	}

	XNFS_printf(2, "Opening %s\n", argv[1]);
	FILE *fin = fopen(argv[1], "rb");
	if (fin == NULL)
	{
		perror("ERROR");
		return -1;
	}

	// printout verbosity level check
	// TODO: add more options
	if (argv[2] != NULL)
	{
		if (argv[2][0] == '-')
		{
			if (argv[2][1] == 'v')
			{
				VerbosityLevel = 2;
				if (argv[2][2] == 'v')
					VerbosityLevel = 3;
			}
		}
	}

	ParseMainSeqData(fin);
	PrintInfo();
	strcpy(OutFileName, argv[1]);
	strcpy(&OutFileName[strlen(OutFileName) - 3], "mid");
	XNFS_printf(1, "Converting main tracks...\n");
	ConvertToMidi(OutFileName);
	if (SeqPack.mdb.NumberOfSequences > 2)
	{
		if (SeqPack.mtdb.SeqTrackOffset)
		{
			XNFS_printf(1, "Converting subtrack 1\n");
			strcpy(MultiSeqName, OutFileName);
			strcpy(&MultiSeqName[strlen(MultiSeqName) - 4], "_SubTrack1.mid");
			ParseNewTrack(fin, SeqPack.mtdb.SeqTrackOffset, MainTBLocation);
			ConvertToMidi(MultiSeqName);
		}

		if (SeqPack.mtdb.SeqTrackOffset2)
		{
			XNFS_printf(1, "Converting subtrack 2\n");
			strcpy(MultiSeqName, OutFileName);
			strcpy(&MultiSeqName[strlen(MultiSeqName) - 4], "_SubTrack2.mid");
			ParseNewTrack(fin, SeqPack.mtdb.SeqTrackOffset2, MainTBLocation);
			ConvertToMidi(MultiSeqName);
		}
	}

	fclose(fin);
    return 0;
}

