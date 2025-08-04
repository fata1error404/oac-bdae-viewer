#pragma once
#include "CPackResReader.h"

//A class to expand CZipReader to a Main Zip + manyPatchReader
// when patch Zip archieve is added, the function will find whether each file in the patch is in the main zip,
// if yes, the entry in the main zip will redirect to the zip archieve's entry.
// if no, a new entry will be added to the main zip (and also point to the zip archieve's entry)

//When Open File, only search main zip's FileList, 
// if the entry's fileDataPosition & 0x80000000 ==0, 
// the file should be located in the main zip
// othewise, decode the zip patch index and zip local index from fileDataPosition and call its openFile(S32 index)

class CPackPatchReader :
	public CPackResReader
{
public:
	CPackPatchReader(const char* filename, bool ignoreCase, bool ignorePaths);

	CPackPatchReader(IReadResFile* file, bool ignoreCase, bool ignorePaths);

	virtual ~CPackPatchReader();
	virtual IReadResFile* openFile(const char* filename);
	virtual bool addPackPatchFile(const char* filename, bool ignoreCase = true, bool ignorePaths = true);

protected:
	std::vector<CPackResReader*> PackPatchFiles;

};
