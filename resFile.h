#ifndef __RESFILE_H_INCLUDED__
#define __RESFILE_H_INCLUDED__

#include <string.h>
#include "access.h"
#include "CPackResReader.h"

// .bdae file header structure
struct FileHeaderData
{
    unsigned int signature;                                 // 4 bytes  file signature – 'BRES' for .bdae file
    unsigned short endianCheck;                             // 2 bytes  byte order mark
    unsigned short version;                                 // 2 bytes  version (?)
    unsigned int sizeOfHeader;                              // 4 bytes  header size in bytes
    unsigned int sizeOfFile;                                // 4 bytes  file size in bytes
    unsigned int numOffsets;                                // 4 bytes  number of entries in the offset table
    unsigned int origin;                                    // 4 bytes  file origin – always '0' for standalone .bdae files (?)
    Access<Access<Access<int>>> offsets;                    // 4 bytes  offset to Offset Data section (in bytes, from the beginning of the file) – should be 80
    Access<int> stringData;                                 // 4 bytes  offset to String Data section
    Access<char> data;                                      // 4 bytes  offset to Data section
    Access<char> relatedFiles;                              // 4 bytes  offset to related file names (?)
    Access<char> removable;                                 // 4 bytes  offset to Removable section
    unsigned int sizeOfRemovableChunk;                      // 4 bytes  size of Removable section in bytes
    unsigned int nbOfRemovableChunks;                       // 4 bytes  number of removable chunks
    unsigned int useSeparatedAllocationForRemovableBuffers; // 4 bytes  1: each removable chunk is loaded into its own separately allocated buffer, 0: all chunks in one shared buffer
    unsigned int sizeOfDynamicChunk;                        // 4 bytes  size of dynamic chunk (?)
};

/*
    We end up with a fully–populated File object whose entire .bdae payload is in memory (header + offset table + string table + data + removable chunks), with every offset “fix‑up” to real C++ pointers and all embedded strings pulled out into shared‐string instances.
*/

// [TODO] annotate
struct File : public Access<FileHeaderData>
{
    std::vector<std::string> StringStorage;

    void *DataBuffer;
    bool IsValid;
    void *OffsetTable;
    void *StringTable;

    int Size;
    int SizeUnRemovable;
    int SizeOffsetStringTables;
    uint64_t *RemovableBuffersInfo;
    void **RemovableBuffers;
    int SizeRemovableBuffer;
    int NbRemovableBuffers;
    bool UseSeparatedAllocationForRemovableBuffers;
    int SizeDynamic;

    // bad: these static variables prevent multi-threaded loading..
    static char *ExternalFilePtr[2];
    static int ExternalFileOffsetTableSize[2];
    static int ExternalFileStringTableSize[2];
    static int SizeOfHeader;
    static bool ExtractStringTable;

    File() : IsValid(false), OffsetTable(NULL) {}

    File(void *ptr, uint64_t *removableBuffersInfo = 0, void **removableBuffers = 0, bool useSeparatedAllocationForRemovableBuffers = false, void *offsetTable = NULL, void *stringTable = NULL)
        : Access<FileHeaderData>(ptr),
          DataBuffer(ptr),
          IsValid(false),
          OffsetTable(offsetTable),
          StringTable(stringTable),
          RemovableBuffersInfo(removableBuffersInfo),
          RemovableBuffers(removableBuffers),
          UseSeparatedAllocationForRemovableBuffers(useSeparatedAllocationForRemovableBuffers)
    {
        if (ptr)
            IsValid = (Init() == 0);
    }

    int Init();

    int Init(IReadResFile *file);
};

#endif
