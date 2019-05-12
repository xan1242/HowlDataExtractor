# Naughty Dog HOWL information
This mess of notes describes the HOWL's data structure, still unfinished, lots of unknown stuff.
Everything was being done using the review copy of the game but you can use whichever version you like.

Help is wanted to get this format dissected properly! :)

## DATA OFFSET ARRAYS

Since PS1's CPU reads in 16-bit words, most of the (large) values are bitshifted to the left and squeezed inside of a 16-bit value.
HOWL format uses arrays of 16-bit values (called tables here wrongly) to store offsets to the data it requires.

The game during the HOWL loading process calculates these and stores them in memory.
It references them as an array of 16-bit values and accesses them before it initiates any data copy.

There seem to be multiple arrays here, possibly for sound effects, but I've mostly focused here on music.
For the music we need 2 arrays - SequencePackOffsetTable and SampleBlockOffsetTable called here.
There are more sample blocks than there are sequence packs, that's again probably due to the sound effects.
However, each sequence pack seems to have their dedicated sample block (referred to by +1, so desert2, or Dingo Canyon will use sequence 0 and sample block 1).

### ACCESSING ARRAYS

Arrays are sorted in order, so size calculation is fairly easy for what we need.
To get the arrays, follow the following notes (similarly you can take a peek at the code, ParseData function):
```
data1:
@0x10 some value, 2 bytes, shifted left by 2 bits
@0x14 some value, 2 bytes, shifted left by 3 bits

0x28 + shifted value at @0x10 = (some offset to something)
(some offset to something) + shifted value at @0x14 = (some offset to something2)

data2:
@0x18 some value, 2 bytes, shifted left by 3 bits
@0x1C some value, 2 bytes, shifted left by 1 bit
@0x20 some value, 2 bytes, shifted left by 1 bit

(some offset to something2) + shifted value at @0x18 = SampleBlockOffsetTable
SampleBlockOffsetTable + shifted value at @0x1C = SequencePackOffsetTable
SequencePackOffsetTable + shifted value at @0x20 = (some offset to something3)
```
## CALCULATING OFFSETS FROM THE ARRAYS

The offset calculation process is very simple:

- Take one value (remember, 16-bit we're playing with here)
- Bitshift to left by 11 bits and store the result to a 32-bit variable/register
- Voila, you've got yourself an absolute file offset to the sampleblock/sequencepack!
 
(the game code likes to do a +1 before it bitshifts thanks to MIPS shenanigans)

## HOWL SEQUENCE PACKS
The sequence structure seems fairly custom, but of course, it's based on MIDI, so it shares a lot in common.
I've broken it up into multiple structures which will (hopefully) make it easier to understand.

### SEQUENCE PACK STRUCTS
```
struct HowlSequencePack
{
	MainDataBlock mdb;
	InstrumentInfoDataBlock iidb[InstrumentCount];
	TrackInfoDataBlock tidb[TrackCount]; // maybe not just for drums, but for samples in general
	MultiTrackDataBlock mtdb; // invincibility music...
	TempoBlock tb;
	short int TrackOffsets[TrackCount + 1]; // maybe wrong... relative offsets to the beginning of the data (sequencedata here)
	unsigned char sequencedata[any];
}

struct MainDataBlock (sizeof: 0x8)
{
	short int TotalFileSize;
	short int unk2;
	unsigned char InstrumentCount; // each size 0xC
	unsigned char TrackCount; // each size 0x8
	short int unk3;
}

struct InstrumentInfoDataBlock (sizeof: 0xC)
{
	unsigned char unk1;
	unsigned char volume;
	short int unk2; 
	short int pitchthing; // related to pitch calculations, unknown as of yet, high byte can change semitones, low byte fine pitch?
	short int InstrumentSampleOffset; // instrument sample offset? changing the high byte to 0 causes countdown horn
	short int ADSR; // ADSR, high byte attack, low byte release, signed, somewhere in between is decay and sustain
	short int unk6; // unknown, really unknown
}

struct TrackInfoDataBlock (sizeof: 8)
{
	unsigned char unk1;
	unsigned char volume;
	short int pitchthing;
	short int unk2;
	short int unk3;
}

struct MultiTrackDataBlock (sizeof: 8) // used to point to invincibility music or something idk.
{
	short int unk1; // offsets tempo info?
	short int InvincibilitySeqTrackOffsetAku;
	short int InvincibilitySeqTrackOffsetUka;
	short int unk4;
}

struct TempoBlock (sizeof: 8)
{
	unsigned char bitshifter; // to left
	unsigned char unk1;
	short int TempoBPM;
	short int Divider_Unknown; // usually is 120, not divider, can be swapped with TempoBPM
	short int unk2;
}
```
### SEQUENCE TRACKS
I haven't dissected the tracks yet (here's where I'm completely lost and I'll need help with).
Every track seems to start with 01 00 00 xx (in 3.seq = crash cove and 11.seq = ngin labs at least)

Here's an example of a HOWL track from Crash Cove (3.seq as extracted by the tool, this should be the cowbell drum track):
```
01 00 00 09 00 00 07 6C 9E 78 05 01 74 21 01 01
1B 05 01 6C 3A 01 01 3E 05 01 61 2F 01 01 49 05
01 61 15 01 01 81 1F 05 01 74 21 01 01 1B 05 01
6C 3A 01 01 3E 05 01 61 2F 01 01 67 05 01 6C 20
01 01 76 05 01 74 21 01 01 1B 05 01 6C 3A 01 01
3E 05 01 61 2F 01 01 49 05 01 61 15 01 01 81 1F
05 01 74 21 01 01 1B 05 01 6C 3A 01 01 3E 05 01
61 2F 01 01 67 05 01 6C 20 01 01 76 05 01 74 21
01 01 1B 05 01 6C 3A 01 01 3E 05 01 61 2F 01 01
49 05 01 61 15 01 01 81 1F 05 01 74 21 01 01 1B
05 01 6C 3A 01 01 3E 05 01 61 2F 01 01 67 05 01
6C 20 01 01 76 05 01 74 21 01 01 1B 05 01 6C 3A
01 01 3E 05 01 61 2F 01 01 49 05 01 61 15 01 01
81 1F 05 01 74 21 01 01 1B 05 01 6C 3A 01 01 3E
05 01 61 2F 01 01 67 05 01 6C 20 01 01 81 96 76
05 01 74 21 01 01 1B 05 01 6C 3A 01 01 3E 05 01
61 2F 01 01 49 05 01 61 15 01 01 81 1F 05 01 74
21 01 01 1B 05 01 6C 3A 01 01 3E 05 01 61 2F 01
01 67 05 01 6C 20 01 01 76 05 01 74 21 01 01 1B
05 01 6C 3A 01 01 3E 05 01 61 2F 01 01 49 05 01
61 15 01 01 81 1F 05 01 74 21 01 01 1B 05 01 6C
3A 01 01 3E 05 01 61 2F 01 01 67 05 01 6C 20 01
01 76 05 01 74 21 01 01 1B 05 01 6C 3A 01 01 3E
05 01 61 2F 01 01 49 05 01 61 15 01 01 81 1F 05
01 74 21 01 01 1B 05 01 6C 3A 01 01 3E 05 01 61
2F 01 01 67 05 01 6C 20 01 01 76 05 01 74 21 01
01 1B 05 01 6C 3A 01 01 3E 05 01 61 2F 01 01 49
05 01 61 15 01 01 81 1F 05 01 74 21 01 01 1B 05
01 6C 3A 01 01 3E 05 01 61 2F 01 01 67 05 01 6C
20 01 01 9D 7E 03 00 00
```

### CRASH COVE RELATIVE TRACK OFFSETS (in order of appearance in the file 3.seq)
```
0x138 - congas
0x310 - cowbell
0x4A4 - closed hi hat
0x644 - open hi hat
0x884 - BASS DRUM
0xEBC - ride cym
0x1764 - snare drum
0x1DB4 - tom/conga/idkwtfisthis
0x1EAC - tom 2/conga/idkwtfisthis
0x2004 - main guitar (first melodic track)
0x2544 - guitar 2 (around 1:06 mark)
0x261C - muted guitar (accompanying track to previous one)
0x2680 - steel drums
0x2CAC - bass
0x343C - vibraphone
0 - congas
```

### SEQUENCE TRACK INFO
```
struct HowlSequenceTrack
{
	char drum_indicator; //??? 1 = drum, else melodic
	char unk1;
	char track_start; // track time start? // (delta time or something, increasing this value = track starts later)
	char unk2; // if 0 = no sound, if too large it can crash, perhaps channel priority?
	char key_transpose; // octave transpose, if more than 1 = transpose up, 1 has the same effect as 0, possibly a signed value, haven't checked
	char attack_time; // ? it has a similar effect to track time start but adds fadein (increases attack time)
	char unk3; // changing this value causes a crash, possibly a count or index of something (channel number?)
	char pan; // track's channel panning (adds to the instrument's value)
	unsigned char track_data[any];
}

// most of the data is read in big endian, adopting MIDI's standards
struct track_data // part of it anyway... this can very likely vary in size thanks to different commands
{
	vlv (char or short) delta_time; // BIG ENDIAN VALUE
	char cmd_type; // 05 = note on
	// the rest is variable, some have more values than the other, this is just for note on event
	char cmd_value; // note value, which note to play
	char cmd_value2; // key velocity
}
```

As is noted above, different command types probably have different sizes, above is described note on
Every command should start with a delta time just like in MIDI

COMMAND LIST:

0x05 = note on, size 4 bytes

## HOWL SAMPLE BLOCKS
Haven't taken a look at it yet... TODO
