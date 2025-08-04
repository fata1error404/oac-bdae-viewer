#include <iostream>
#include <iomanip>
#include <cstdint>
#include "Logger.h"
#include "resFile.h"
#include "PackPatchReader.h"

int File::SizeOfHeader = 0;

bool File::ExtractStringTable = true;

// metadata storage based on reference type: index 0 = internal, index 1 = external
int File::ExternalFileOffsetTableSize[2] = {0, 0};
int File::ExternalFileStringTableSize[2] = {0, 0};
char *File::ExternalFilePtr[2] = {0, 0};

//! Reads raw binary data from .bdae file and loads its sections into memory.
// __________________________________________________________________________

int File::Init(IReadResFile *file)
{
    LOG("[Init] Starting File::Init..\n");
    LOG("---------------");
    LOG("[Init] PART 1. \n       Reading raw binary data from .bdae file and loading its sections into memory.");
    LOG("---------------\n\n");

    // 1. Read Header data as a structure.
    Size = file->getSize();
    int headerSize = sizeof(struct FileHeaderData);
    struct FileHeaderData *header = new FileHeaderData;

    LOG("[Init] Header size (size of struct): ", headerSize);
    LOG("[Init] File size (length of file): ", Size);
    LOG("[Init] File name: ", file->getFileName());
    LOG("\n[Init] At position ", file->getPos(), ", reading header..");

    file->read(header, headerSize);

    LOG("_________________");
    LOG("\nFile Header Data\n");
    std::ostringstream hexStream;
    hexStream << "Signature: " << std::hex << ((char *)&header->signature)[0] << ((char *)&header->signature)[1] << ((char *)&header->signature)[2] << ((char *)&header->signature)[3] << std::dec;
    LOG(hexStream.str());
    LOG("Endian check: ", header->endianCheck);
    LOG("Version: ", header->version);
    LOG("Header size: ", header->sizeOfHeader);
    LOG("File size: ", header->sizeOfFile);
    LOG("Number of offsets: ", header->numOffsets);
    LOG("Origin: ", header->origin);
    LOG("\nSection offsets  ");
    LOG("Offset Data:   ", header->offsets.m_offset);
    LOG("String Data:   ", header->stringData.m_offset);
    LOG("Data:          ", header->data.m_offset);
    LOG("Related files: ", header->relatedFiles.m_offset);
    LOG("Removable:     ", header->removable.m_offset);
    LOG("\nSize of Removable Chunk: ", header->sizeOfRemovableChunk);
    LOG("Number of Removable Chunks: ", header->nbOfRemovableChunks);
    LOG("Use separated allocation: ", ((header->useSeparatedAllocationForRemovableBuffers > 0) ? "Yes" : "No"));
    LOG("Size of Dynamic Chunk: ", header->sizeOfDynamicChunk);
    LOG("________________________\n");

    // 2. Search for related files.
    unsigned int beginOfRelatedFiles = header->relatedFiles.m_offset - header->origin;

    if (header->origin == 0)
    {
        LOG("[Init] At position ", beginOfRelatedFiles, ", checking for related filenames..");

        // read name size of the related file
        int sizeOfName = 0;
        file->seek(beginOfRelatedFiles);
        file->read(&sizeOfName, 4);

        unsigned char *bytes = reinterpret_cast<unsigned char *>(&sizeOfName);
        LOG("[Init] Size of related filename: ");

        std::ostringstream hexStream;
        hexStream << std::hex << std::setfill('0');

        for (int i = 0; i < 4; ++i)
            hexStream << std::setw(2) << static_cast<int>(bytes[i]) << " ";

        hexStream << std::dec;

        LOG(hexStream.str());
        LOG("(", sizeOfName, " byte)");

        // validity check: name size should not exceed the limit for filename length
        if (sizeOfName > 256)
            LOG("[Init] Warning: sizeOfName exceeds buffer size!");

        // validity check: name is real (size 1 means none)
        if (sizeOfName > 1)
        {
            // read name of the related file
            beginOfRelatedFiles += 4;
            char relatedFileName[256];
            file->seek(beginOfRelatedFiles);
            file->read(relatedFileName, (sizeOfName + 3) & ~3); // align to next 4 bytes, as name size may not be a multiple of 4

            LOG("[Init] Filename: ", relatedFileName);

            // load related bdae file (?)
            // collada::CResFileManager::getInst()->get(buff, NULL, true);
        }
        else
            LOG("[Init] Invalid name. No related files found.");
    }

    // 3. Initialize File struct variables and allocate memory for reading rest of the file.
    int sizeOffsetTable;
    int sizeStringTable;
    int sizeDynamicContent;

    sizeOffsetTable = header->numOffsets * sizeof(uint64_t);
    sizeStringTable = (ExtractStringTable ? header->data.m_offset - header->stringData.m_offset : 0);
    SizeRemovableBuffer = header->sizeOfRemovableChunk;
    sizeDynamicContent = header->sizeOfDynamicChunk;

    SizeUnRemovable = Size - sizeOffsetTable - sizeStringTable - SizeRemovableBuffer - sizeDynamicContent;
    NbRemovableBuffers = header->nbOfRemovableChunks;
    UseSeparatedAllocationForRemovableBuffers = (header->useSeparatedAllocationForRemovableBuffers > 0) ? true : false;

    char *offsetBuffer = new char[sizeOffsetTable];                               // temp buffer for offset table
    char *stringBuffer = (ExtractStringTable ? new char[sizeStringTable] : NULL); // temp buffer for string table
    char *buffer = (char *)malloc(SizeUnRemovable);                               // main buffer

    memcpy(buffer, header, headerSize); // copy header

    // 4. Read offset, string, data and related files sections as a raw binary data.
    file->seek(headerSize);
    LOG("\n[Init] At position ", file->getPos(), ", reading offset ", (sizeStringTable ? "and string tables.." : "table.."));

    file->read(offsetBuffer, sizeOffsetTable);

    if (sizeStringTable)
        file->read(stringBuffer, sizeStringTable);

    LOG("\n[Init] At position ", file->getPos(), ", reading rest of the file (up to the removable section)..");
    file->read(&buffer[headerSize], SizeUnRemovable - headerSize); // insert after header

    // 5. Read removable chunks.
    RemovableBuffers = NULL;

    if (SizeRemovableBuffer > 0)
    {
        // read size / offset pairs for each removable chunk
        LOG("\n[Init] At position ", file->getPos(), ", reading removable section info..");
        RemovableBuffersInfo = new uint64_t[NbRemovableBuffers * 2];
        file->read(RemovableBuffersInfo, NbRemovableBuffers * 2 * sizeof(uint64_t));

        LOG("\n_____________________\n");
        LOG("Removable chunks info");
        LOG("[#] (size, offset)");

        for (int i = 0; i < NbRemovableBuffers; ++i)
            LOG("[", i + 1, "] ", "(", RemovableBuffersInfo[i * 2], ", ", RemovableBuffersInfo[i * 2 + 1], ")");

        LOG("________________\n");

        // read chunks data
        LOG("[Init] At position ", file->getPos(), ", reading removable section data..");
        RemovableBuffers = new void *[NbRemovableBuffers];

        if (UseSeparatedAllocationForRemovableBuffers)
        {
            // separated allocation mode: read each chunk into its own buffer
            for (int i = 0; i < NbRemovableBuffers; ++i)
            {
                uint64_t bufSize = RemovableBuffersInfo[i * 2];
                RemovableBuffers[i] = new char[bufSize];
                file->read(RemovableBuffers[i], bufSize);
            }
        }
        else
        {
            // single-block mode: read all chunks into one large buffer

            /*
                RemovableBuffers[0]             → pointer to the entire data block
                RemovableBuffers[i] (for i > 0) → pointer into that block at the start of chunk i
            */
            uint64_t totalDataSize = SizeRemovableBuffer - (NbRemovableBuffers * 2 * sizeof(uint64_t));
            RemovableBuffers[0] = new char[totalDataSize];
            file->read(RemovableBuffers[0], totalDataSize);

            uint64_t baseOffset = RemovableBuffersInfo[1];

            // assign pointers
            for (int i = 1; i < NbRemovableBuffers; ++i)
            {
                uint64_t chunkOffset = RemovableBuffersInfo[i * 2 + 1];
                RemovableBuffers[i] = (char *)RemovableBuffers[0] + (chunkOffset - baseOffset);
            }
        }
    }

    LOG("[Init] Stopped reading ", file->getFileName(), " at position ", file->getPos(), " (end of file).");

    delete header;

    // run the real init (apply offset correction and do string extraction from offset and string tables)
    *this = File(buffer,
                 RemovableBuffersInfo,
                 RemovableBuffers,
                 UseSeparatedAllocationForRemovableBuffers,
                 offsetBuffer,
                 stringBuffer);

    // causes wrong offset for the first 2 offset entries
    delete[] offsetBuffer;
    OffsetTable = NULL;

    delete[] stringBuffer;
    StringTable = NULL;

    // delete[] RemovableBuffersInfo;
    // RemovableBuffersInfo = NULL;

    return IsValid != 1;
}

//! MAIN initialization. Resolves all relative offsets in the loaded .bdae file, converting them to direct pointers while handling internal vs. external references, string data extraction, and removable chunks.
// ______________________________________________________________________________________________________________________________________________________________________________________________________________

int File::Init()
{
    LOG("\n\n\n\n---------------");
    LOG("[Init] PART 2. \n       Resolving all relative offsets in the loaded .bdae file: convert them to direct pointers, handle internal vs. external references, string data extraction, and removable chunks.");
    LOG("---------------\n\n");

    // 6. Prepare for file processing: retrieve the Header struct from memory and initialize File struct variables (we replaced the File object with a new one by calling the second Init(), so this is the actual initialization).
    FileHeaderData *header = ptr();

    Size = header->sizeOfFile;

    SizeOffsetStringTables = 0;

    if (OffsetTable)
        SizeOffsetStringTables += header->numOffsets * sizeof(uint64_t);
    if (StringTable && ExtractStringTable)
        SizeOffsetStringTables += header->data.m_offset - header->stringData.m_offset;

    SizeRemovableBuffer = header->sizeOfRemovableChunk;
    SizeDynamic = header->sizeOfDynamicChunk;
    SizeUnRemovable = Size - SizeRemovableBuffer - SizeDynamic;
    NbRemovableBuffers = header->nbOfRemovableChunks;

    /* save the header pointer into a different array index based on whether it's internal or external
       (highest bit of the origin field is used as a flag):

        0 → internal – referenced data is within the same .bdae file
        1 → external – referenced data is in some related file
    */
    ExternalFilePtr[(header->origin) >> 31] = reinterpret_cast<char *>(header);

    // validity check: file signature
    if (((char *)&header->signature)[0] != 'B' ||
        ((char *)&header->signature)[1] != 'R' ||
        ((char *)&header->signature)[2] != 'E' ||
        ((char *)&header->signature)[3] != 'S')
    {
        LOG("[Init] Warning: wrong signature!");
        return -1;
    }

    // validity check: proceed only if the header is valid and the file hasn't already been marked as processed (highest bit of the version field is used as a flag)
    if (header && (header->version & 0x8000) == 0)
    {
        header->version |= 0x8000; // set the high bit of the version by doing a bitwise OR with 0x8000
        LOG("[Init] Passed validity checks! This file hasn't been processed yet. Proceeding with configuration..");

        // 7a. There is a temporary, separate, deletable buffer for the offset table (allocated in the first Init()). We have to process each table entry, correcting it, retrieving string data, and then converting its contained relative offset to a direct pointer.
        if (OffsetTable)
        {
            LOG("[Init] Using a temporary buffer for offset table. Retrieving the string data, applying offset correction, and performing offset-to-pointer conversion..");

            SizeOfHeader = header->sizeOfHeader;
            (&header->offsets)[0] = OffsetTable; // override the address of the offset table in the Header struct to point to the temp buffer for offset table (this allows to perform all pointer fix-ups against our own copy and safely free or reallocate it without touching the original memory block mapped from the .bdae file)

            // sizes and pointers of the offset / string tables
            int sizeOffsetTable = header->numOffsets * sizeof(uint64_t);
            int sizeStringTable = (ExtractStringTable ? header->data.m_offset - header->stringData.m_offset : 0);
            unsigned int offsetTableEnd = sizeOffsetTable + SizeOfHeader;
            unsigned int stringTableEnd = ExtractStringTable ? offsetTableEnd + sizeStringTable : offsetTableEnd;
            ExternalFileOffsetTableSize[(header->origin) >> 31] = offsetTableEnd;
            ExternalFileStringTableSize[(header->origin) >> 31] = stringTableEnd;
            char *stringTableStartPtr = (char *)StringTable;

            // loop through each entry in the offset table
            for (unsigned int i = 0; i < header->numOffsets; ++i)
            {
                Access<Access<int>> &offset = header->offsets[i]; // i-th offset table entry: an outer Access<Access<int>> object that stores the offset to an inner Access<int> object, which itself holds the offset to actual data

                /* FIRST PASS. Outer pointer.
                   Process offptr handling cases where it points to different sections of the .bdae file.
                   ────────────────────────────────────────────────────────────────────────────────────── */

                char *origin = reinterpret_cast<char *>(header);                                 // pointer to the start of the current file’s memory block
                unsigned int originoff = header->origin;                                         // base offset used as a reference to resolve relative offsets in the file
                uintptr_t offptr = reinterpret_cast<uintptr_t>(offset.ptr()) - (header->origin); // outer offset: relative offset from the file’s origin to the target pointer of this entry (had to change unsigned int to uintptr_t variable type to silence the pointer arithmetic warning)
                unsigned int ote = offsetTableEnd;
                unsigned int ste = stringTableEnd;
                bool external = false; // flag to indicate if this entry refers to an external memory chunk

                // if this entry’s target lies beyond the bounds of the current .bdae file, adjust values to "external mode"
                if (offptr > (unsigned int)Size)
                {
                    origin = ExternalFilePtr[offptr >> 31];
                    originoff = ((offptr >> 31) << 31); // set base offset to 0x80000000
                    offptr += header->origin;           // convert to absolute offset
                    ote = ExternalFileOffsetTableSize[offptr >> 31];
                    ste = ExternalFileStringTableSize[offptr >> 31];
                    external = true; // mark that we are now resolving an external reference
                }

                // LOG("[", i + 1, "] ", offptr);

                // if this entry’s target lies after the Offset Data section
                if (offptr >= ote)
                {
                    // String Data section
                    if (offptr < stringTableEnd && StringTable)
                    {
                        unsigned int size = *reinterpret_cast<unsigned int *>(stringTableStartPtr + (offptr - ote) - sizeof(unsigned int)); // (?)
                        std::string str(stringTableStartPtr + (offptr - ote), size);                                                        // (?)

                        StringStorage.push_back(str);

                        const char *cstr = StringStorage.back().c_str();                                 // get a C-string pointer
                        Access<Access<int>> newPtr(const_cast<void *>(static_cast<const void *>(cstr))); // wrap the pointer in an outer Access<Access<int>> object
                        offset = newPtr;                                                                 // this offset table entry now points to the inner object (we get the access to the inner pointer, but not yet to the actual string data)
                    }
                    // Removable section
                    else if (offptr > (unsigned int)SizeUnRemovable)
                    {
                        int nb = ((offptr - SizeUnRemovable) - sizeof(unsigned int)) / (sizeof(unsigned int) * 4); // determine which removable buffer this offset entry points to

                        // if the computed removable buffer number (index) is out of range, resolve it
                        if (nb > NbRemovableBuffers)
                        {
                            // search linearly for the correct removable buffer whose [start, end] contains offptr
                            int nb1 = 0;
                            while (nb1 < NbRemovableBuffers - 1)
                            {
                                if (offptr > RemovableBuffersInfo[nb1 * 2 + 1] && offptr < RemovableBuffersInfo[nb1 * 2 + 3])
                                    break;

                                nb1++;
                            }

                            void *base = (char *)((char *)RemovableBuffers[nb1] - (char *)RemovableBuffersInfo[nb1 * 2 + 1]); // pointer to the beginning of the nb1-th chunk in the Removable section
                            offset.OffsetToPtr(base);

                            uintptr_t offptrptr = reinterpret_cast<uintptr_t>(offset.ptr()->ptr()) - (header->origin); // relative offset from the file’s origin to the actual data of this chunk (i.e., the inner pointer stored at the target entry, which points to data within a removable chunk)

                            // if this pointer also falls in the Removable section, resolve it
                            if (offptrptr > (unsigned int)SizeUnRemovable)
                            {
                                int nb2 = 0;
                                while (nb2 < NbRemovableBuffers - 1)
                                {
                                    if (offptrptr > RemovableBuffersInfo[nb2 * 2 + 1] && offptrptr < RemovableBuffersInfo[nb2 * 2 + 3])
                                        break;

                                    nb2++;
                                }

                                void *base = (char *)((char *)RemovableBuffers[nb2] - (char *)RemovableBuffersInfo[nb2 * 2 + 1]);
                                offset.ptr()->OffsetToPtr(base);
                                continue;
                            }

                            // offset.OffsetToPtr((char *)RemovableBuffersInfo[nb * 2 + 1] - offptr);
                        }
                        // computed removable buffer number was valid but no correction is desired (it is commented out in the source code)
                        else
                        {
                            void *base = (char *)((char *)RemovableBuffers[nb] - (char *)RemovableBuffersInfo[nb * 2 + 1]);
                            offset.OffsetToPtr(base);
                            continue;
                        }
                    }
                    // Data, Related Files sections: no extra correction
                    else
                    {
                        void *base = origin - (ste - SizeOfHeader) - originoff; // pointer to the beginning of the Data section (not sure why we substract header size)
                        offset.OffsetToPtr(base);
                    }
                }
                // Header section (and Offset Data? though it's unlikely any offset entries point back to the offset table): no extra correction
                else
                {
                    void *base = origin - originoff; // pointer to "zero" offset – the beginning of the Header section
                    offset.OffsetToPtr(base);
                }

                // for external reference, we skip the rest of the loop and go to the next iteration (reference file might not be loaded yet and must be initialized independently)
                if (external)
                    continue;

                /* SECOND PASS. Inner pointer.
                   Process offptrptr in the same way, with minor changes to the logic.
                   ─────────────────────────────────────────────────────────────────── */

                if (i > 0) // first offset table entry skipped
                {
                    origin = reinterpret_cast<char *>(header);
                    originoff = header->origin;
                    uintptr_t offptrptr = reinterpret_cast<uintptr_t>(offset.ptr()->ptr()) - (header->origin); // inner offset: relative offset from the file’s origin to the target data of this entry
                    ote = offsetTableEnd;
                    ste = stringTableEnd;

                    if (offptrptr > (unsigned int)Size)
                    {
                        origin = ExternalFilePtr[offptrptr >> 31];
                        originoff = (offptrptr >> 31) << 31;
                        offptrptr += header->origin;
                        ote = ExternalFileOffsetTableSize[offptrptr >> 31];
                        ste = ExternalFileStringTableSize[offptrptr >> 31];
                    }

                    // LOG("[", i + 1, "] ", offptrptr);

                    if (offptrptr >= ote)
                    {
                        // logic changed; we now skip the first string table entry (likely to exclude the header string)
                        if (offptrptr != ote && offptrptr < stringTableEnd)
                        {
                            unsigned int size = *reinterpret_cast<unsigned int *>(stringTableStartPtr + (offptrptr - ote) - sizeof(unsigned int)); // retrieve the string length from the 4 bytes located just before the string itself
                            std::string str(stringTableStartPtr + (offptrptr - ote), size);                                                        // RETRIEVE THE STRING

                            StringStorage.push_back(str);

                            const char *cstr = StringStorage.back().c_str();
                            Access<int> newPtr(const_cast<void *>(static_cast<const void *>(cstr))); // logic changed: wrap the pointer in an inner Access<int> object
                            *static_cast<Access<int> *>(offset.ptr()) = newPtr;                      // logic changed: we now have direct access to the string stored in string storage
                        }
                        else if (offptrptr > (unsigned int)SizeUnRemovable)
                        {
                            int nb = 0;
                            while (nb < NbRemovableBuffers)
                            {
                                if (offptrptr == RemovableBuffersInfo[nb * 2 + 1]) // logic changed: we now check if the pointer exactly matches the offset of the start of a removable chunk, instead of being within its bounds (this is because, in the second pass, we are resolving only direct references)
                                    break;

                                nb++;
                            }

                            offset.ptr()->OffsetToPtr((char *)RemovableBuffers[nb] - offptrptr + sizeof(int));
                        }
                        else
                            offset.ptr()->OffsetToPtr(origin - (ste - SizeOfHeader) - originoff);
                    }
                    else
                        offset.ptr()->OffsetToPtr(origin - originoff);
                }
            }
        }
        /* 7b. This occurs when a temporary buffer is not used. The offset table is in-place — directly in the file’s main memory buffer — no separate deletable buffer (OffsetTable == NULL), so no need to retrieve or correct anything. Simply convert relative offsets to direct pointers. */
        else
        {
            LOG("[Init] No temporary buffer found for offset table, though no retrieval or correction required. Only performing offset-to-pointer conversion..");

            // if the offset table is in-place, then the string table must be as well
            if (StringTable != NULL)
            {
                std::cerr << "Error: StringTable must be NULL\n";
                std::abort();
            }

            // loop through each entry in the offset table
            for (unsigned int i = 0; i < header->numOffsets; ++i)
            {
                Access<Access<int>> &offset = header->offsets[i];

                offset.OffsetToPtr(header); // convert outer pointer

                if (i > 0)
                    offset.ptr()->OffsetToPtr(header); // convert inner pointer
            }
        }
    }

    LOG("\n[Init] Finishing File::Init..\n\n");

    LOG("_____________________");
    LOG("\nExtracted String Data\n");

    for (int i = 0; i < (int)StringStorage.size(); ++i)
        LOG("[", std::setw(2), i + 1, "] \"", StringStorage[i], "\"");

    LOG("_____________________\n");

    return 0;
}
