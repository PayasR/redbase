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
#include <cfloat>

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
	PF_PageHandle* L = chooseLeaf(obj, root);
	PF_PageHandle* LL = NULL;
	if (!pageFull(L))
		insertObject(obj, L);
	else
		LL = splitNode(obj, L);
	adjustTree(L, LL);

}

bool IX_IndexHandle::pageFull(PF_PageHandle* page) {
	char* pdata;
	IX_NodeHeader* head;
	RC err;
	if (!(err = page->GetData(pdata))) {
		head = (IX_NodeHeader*) pdata;
		if (head->type & LEAF) {
			if (head->nodeNum == ENTRY_M)
				return true;
			else
				return false;
		} else {
			if (head->nodeNum == OBJECT_M)
				return true;
			else
				return false;
		}
	}
	cout << "Page Full Subroutine Fail." << endl;
	PF_PrintError(err);
	return false;
}


PF_PageHandle* IX_IndexHandle::chooseLeaf(IX_Object* obj, PF_PageHandle* node) {
	if (checkLeaf(node))
		return node;
	else {
		PF_PageHandle* nextnode = findEnlargeLeast(obj, node);
		return chooseLeaf(obj, node);
	}
}

bool IX_IndexHandle::checkLeaf(PF_PageHandle* node) {
	char* pdata;
	RC err;
	IX_NodeHeader* head;
	if (head = getNodeHead(node)) {
		if (head & LEAF) return true;
		else return false;
	}
	cout << "[IX_IndexHandle::checkLeaf] Get Page Header Error" << endl;
	return false;
}

PF_PageHandle* IX_IndexHandle::findEnlargeLeast(IX_Object* obj, PF_PageHandle* node) {

	IX_NodeHeader* head;
	if (head = getNodeHead(node)) {
		if (head->tyep & LEAF)
			return node;
		else {
			int nodeNum = head->nodeNum;
			IX_Entry* en = (IX_Entry*)((char*) head + sizeof(IX_NodeHeader));
			IX_Entry* minen;
			float minarea = FLT_MAX;
			for (int i = 0; i < nodeNum; ++i, ++en) {
				float diffarea = en->mbr.enlargeTest(obj->point);
				if (diffarea < minarea) {
					minarea = diffarea;
					minen = en;
				}
			}
			trace.push(IX_Trace(node, minen));
			PF_PageHandle* nextPage = new PF_PageHandle();
			if (!fileHandle->GetThisPage(minen->child, *nextPage))
				return nextPage;
			else
				return NULL;
		}

	}
	cout << "[IX_IndexHandle::findEnlargeLeast] Get Page Header Error" << endl;
	return NULL;
}

IX_NodeHeader* IX_IndexHandle::getNodeHead(PF_PageHandle* node) {
	char* pdata;
	if (!(err = node->GetData(pdata)))
		return (IX_NodeHeader*) pdata;
	PF_PrintError(err);
	return NULL;
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
