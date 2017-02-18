//
// File:        ix_indexmanager.cc
// Description: IX_IndexHandle handles indexes
// Author:      Yifei Huang (yifei@stanford.edu)
//

#include <unistd.h>
#include <sys/types.h>
#include "ix.h"
#include "pf.h"
#include <climits>
#include <string>
#include <sstream>
#include <cstdio>
#include "comparators.h"
#include <iostream>
using namespace std;

IX_Manager::IX_Manager(PF_Manager &pfm) {
	pageManager = &pfm;
}

IX_Manager::~IX_Manager() {
	pageManager = NULL;
}

/*
 * Creates a new index given the filename, the index number, attribute type and length.
 */
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                           AttrType attrType, int attrLength) {
	// Get intact file name
	char* indexNostr = (char*) malloc(INT_LENGTH);
	sprintf(indexNostr, "%d", indexNo);
	string file(fileName);
	file += indexNostr;

	cout << "Create Index File: " << file << endl;
	RC err;
	if (err = pageManager->CreateFile(file.c_str()))
		PF_PrintError(err);
	cout << "Finish" << endl << endl;
	return err;

}

/*
 * This function destroys a valid index given the file name and index number.
 */
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {
	char* indexNostr = (char*) malloc(INT_LENGTH);
	sprintf(indexNostr, "%d", indexNo);
	string file(fileName);
	file += indexNostr;

	cout << "Delete Index File: " << file << endl;
	RC err;
	if (err = pageManager->DestroyFile(file.c_str()))
		PF_PrintError(err);
	cout << "Finish" << endl << endl;
}

/*
 * This function, given a valid fileName and index Number, opens up the
 * index associated with it, and returns it via the indexHandle variable
 */
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
                         IX_IndexHandle &indexHandle) {
	char* indexNostr = (char*) malloc(INT_LENGTH);
	sprintf(indexNostr, "%d", indexNo);
	string* file = new string(fileName);
	file->append(indexNostr);

	cout << "Open Index File: " << file << endl;
	PF_FileHandle* filehandle = new PF_FileHandle();
	RC err = pageManager->OpenFile(file.c_str(), *filehandle);
	if (!err) {
		// Initialize index handle
		indexHandle.fileHandle = filehandle;
		indexHandle.fileName = file;
		indexHandle.fileHeaderPage = new PF_PageHandle();
		err = filehandle->GetFirstPage(*indexHandle.fileHeaderPage);
		if (!err) {
			// Get file header
			char* pdata;
			indexHandle.fileHeaderPage->GetData(pdata);
			indexHandle.fileHeader = (PF_FileHdr*) pdata;

			// Apply root page if file is empty
			PF_PageHandle* root;
			if (indexHandle.fileHeader->firstFree == PF_PAGE_LIST_END) {
				if (root = indexHandle.newPage(ROOT | LEAF)) {
					indexHandle.root = root;

					// Mark file header as dirty
					int pn;
					root->GetPageNum(pn);
					indexHandle.fileHeader->firstFree = pn; // Set root page number in file header
					indexHandle.fileHeader->numPages = 1;
					indexHandle.markPageDirty(indexHandle.fileHeaderPage);
				} else
					cout << "[IX_Manager::OpenIndex] Null Page Alloc" << endl;
			} else {
				root = new PF_PageHandle();
				if (!indexHandle.fileHandle->GetThisPage(indexHandle.fileHeader->firstFree, *root))
					indexHandle.root = root;
				else
					cout << "[IX_Manager::OpenIndex] GetThisPage Error" << endl;
			}
			cout << "PF_FileHeader: " << indexHandle.fileHeader->firstFree << ", " <<
			     indexHandle.fileHeader->numPages << endl;
		} else
			PF_PrintError(err);
	} else
		PF_PrintError(err);
	cout << "Finish" << endl << endl;
	return err;
}

/*
 * Given a valid index handle, closes the file associated with it
 */
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {
	cout << "Close Index File: " << *indexHandle.fileName << endl;
	RC err = pageManager->CloseFile(*indexHandle.fileHandle);
	delete indexHandle.fileHandle;
	delete indexHandle.fileName;
	delete indexHandle.fileFirstPage;
	delete indexHandle.root;
	if (err) PF_PrintError(err);
	cout << "Finish" << endl << endl;
	return err;
}
