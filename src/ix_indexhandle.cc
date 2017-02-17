//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle handles manipulations within the index
// Author:      <Your Name Here>
//

#include <unistd.h>
#include <sys/types.h>
#include "ix.h"
#include "pf.h"
#include "comparators.h"
#include <cstdio>

IX_IndexHandle::IX_IndexHandle() {
	fileHandle = NULL;
	fileFirstPage = NULL;
	fileName = NULL;
	hdr = NULL;
	ENTRY_M = (PF_PAGE_SIZE - sizeof(IX_Header)) / sizeof(IX_Entry);
	ENTRY_m = (ENTRY_M + 1) / 2;
	OBJECT_M = (PF_PAGE_SIZE - sizeof(IX_Header)) / sizeof(IX_Object);
	OBJECT_m = (OBJECT_M + 1) / 2;
}

IX_IndexHandle::~IX_IndexHandle() {

}

RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid) {
	IX_Point* point = (IX_Point*) pData;
	IX_Object* obj = new IX_Object(*point, rid);
	PF_PageHandle* L = chooseLeaf(obj);
	PF_PageHandle* LL = NULL;
	if (!pageFull(L))
		insertObject(obj, L);
	else
		LL = splitNode(L);
	adjustTree(L, LL);

}

bool IX_IndexHandle::pageFull(PF_PageHandle* page) {

}

PF_PageHandle* IX_IndexHandle::chooseLeaf(IX_Object* obj) {

}

RC IX_IndexHandle::insertObject(IX_Object* obj, PF_PageHandle* L) {

}

PF_PageHandle* IX_IndexHandle::splitNode(PF_PageHandle* L) {

}

RC IX_IndexHandle::adjustTree(PF_PageHandle* L, PF_PageHandle* LL) {

}

RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid) {
	// Implement this
}

RC IX_IndexHandle::ForcePages() {
	// Implement this
}
