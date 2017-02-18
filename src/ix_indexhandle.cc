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
	ENTRY_M = (PF_PAGE_SIZE - sizeof(IX_Header)) / sizeof(IX_Entry) - 1;
	ENTRY_m = ENTRY_M / 2;
}

IX_IndexHandle::~IX_IndexHandle() {

}

PF_PageHandle*  IX_IndexHandle::newPage(int& type) {
	PF_PageHandle* newpage = new PF_PageHandle();
	if (!fileHandle->AllocatePage(*newpage)) {
		IX_NodeHeader* nhead;
		if (nhead = indexHandle.getNodeHead(newpage)) {
			int pn;
			nhead->nodeNum = 0;
			nhead->type = type;
			nhead->sortNeed = false;
			nhead->enlargeNeed = false;
			newpage->GetPageNum(pn);
			fileHandle->MarkDirty(pn);
			return newpage;
		}
	}
	return NULL;
}

RC IX_IndexHandle::InsertEntry(void* pData, const RID& rid) {
	IX_MBR* mbr = (IX_MBR*) pData;
	IX_Object* obj = new IX_Object(mbr, rid);
	PF_PageHandle* L = chooseLeaf(obj, root);
	PF_PageHandle* LL = NULL;
	insertObject<IX_Object>(obj, L);
	if (pageFull(L))
		LL = splitNode(obj, L);
	adjustTree(L, LL);
	/*
	int size = stack.size();
	for (int i = 0; i < size; ++i)
		stack.pop();
	*/
	delete obj;
	delete L;
	if (LL != NULL)
		delete LL;
}


PF_PageHandle* IX_IndexHandle::chooseLeaf(IX_Object* obj, PF_PageHandle* node) {
	if (checkLeaf(node))
		return node;
	else {
		PF_PageHandle* nextnode = findEnlargeLeast(obj, node);
		return chooseLeaf(obj, node);
	}
}


PF_PageHandle* IX_IndexHandle::findEnlargeLeast(IX_Object* obj, PF_PageHandle* node) {
	IX_NodeHeader* nhead;
	if (nhead = getNodeHead(node)) {
		int nodeNum = nhead->nodeNum;
		IX_Entry* enlist = getDataList<IX_Entry>(nhead);
		IX_Entry* minen = NULL;
		float minarea = FLT_MAX;
		float diffarea;
		for (int i = 0; i < nodeNum; ++i, ++enlist) {
			diffarea = enlist->mbr.enlargeTest(obj->mbr);
			if (diffarea < minarea) {
				minarea = diffarea;
				minen = enlist;
			}
		}
		if (abs(minarea) <= 1e5)
			nhead->enlargeNeed = false;
		else
			nhead->enlargeNeed = true;
		trace.push(IX_Trace(node, minen));
		PF_PageHandle* nextPage = new PF_PageHandle();
		if (!fileHandle->GetThisPage(minen->child, *nextPage))
			return nextPage;
		else
			return NULL;
	}
	cout << "[IX_IndexHandle::findEnlargeLeast] Get Page Header Error" << endl;
	return NULL;
}


template<class T>
T* getDataList(IX_NodeHeader* nhead) {
	return (T*)((char*) nhead + sizeof(IX_NodeHeader));
}

template<class T>
RC IX_IndexHandle::insertObject(T* obj, PF_PageHandle* L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	if (nhead != NULL) {
		T* objList = getDataList<T>(nhead);
		objList[nhead->nodeNum++] = *obj;
		nhead->sortNeed = true;
		return markPageDirty(L);
	}
	return PF_INVALIDPAGE;
}


bool IX_IndexHandle::pageFull(PF_PageHandle* page) {
	char* pdata;
	IX_NodeHeader* nhead;
	if (nhead = getNodeHead(page)) {
		if (nhead->nodeNum > ENTRY_M)
			return true;
		else
			return false;

	}
	cout << "[IX_IndexHandle::pageFull] Page invalid" << endl;
	return false;
}


bool IX_IndexHandle::checkLeaf(PF_PageHandle* node) {
	char* pdata;
	RC err;
	IX_NodeHeader* nhead;
	if (nhead = getNodeHead(node)) {
		if (nhead & LEAF) return true;
		else return false;
	}
	cout << "[IX_IndexHandle::checkLeaf] Get Page Header Error" << endl;
	return false;
}


IX_NodeHeader* IX_IndexHandle::getNodeHead(PF_PageHandle* node) {
	char* pdata;
	if (!(err = node->GetData(pdata)))
		return (IX_NodeHeader*) pdata;
	cout << "[IX_IndexHandle::getNodeHead] Get Node Header Error" << endl;
	PF_PrintError(err);
	return NULL;
}

// Need Template
template<class T>
PF_PageHandle* IX_IndexHandle::splitNode(T* obj, PF_PageHandle  L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	T* enList = getDataList<T>(nhead);
	if (nhead->sortNeed) {
		sort(enList, enList + nhead->nodeNum, sortMBR<T>);
		nhead->sortNeed = false;
	}

	// Initialize new page
	PF_PageHandle* LL = NULL;
	if (checkLeaf(L))
		LL = newPage(LEAF);
	else
		LL = newPage(NONLEAF);
	IX_NodeHeader* nheadLL = getNodeHead(LL);

	// Copy data from L for split
	int remain = nhead->nodeNum / 2;
	T* enSpList = getDataList<T>(nheadLL);
	nheadLL->nodeNum = nhead->nodeNum - remain;
	memcpy(enSpList, enList + remain, sizeof(T) * nheadLL->nodeNum);
	nhead->nodeNum = remain;

	// Mark LL as dirty
	markPageDirty(LL);

	// Mark file header as dirty
	++fileHeader->numPages;
	markPageDirty(fileHeaderPage);

	return LL;
}


RC IX_IndexHandle::markPageDirty(PF_PageHandle* page) {
	int pn, err;
	if (!(err = page->GetPageNum(pn)))
		return fileHandle->MarkDirty(pn);
	return err;
}


RC IX_IndexHandle::adjustTree(PF_PageHandle* L, PF_PageHandle* LL) {
	if (LL == NULL) {
		if (!stack.empty()) {
			// Update based on Entry
			PF_PageHandle* par;
			IX_Entry* enpar;
			IX_NodeHeader* nhead;
			int size = stack.size();
			for (int i = 0; i < size; ++i) {
				par = stack.top().tpage;
				enpar = stack.top().tentry;
				nhead = getNodeHead(par);
				if (nhead->enlargeNeed) {
					if (checkLeaf(L))
						updateMBR<IX_Object>(enpar->mbr, L);
					else
						updateMBR<IX_Entry>(enpar->mbr, L);
					nhead->sortNeed = true;
				}
				delete L;
				L = par;
				stack.pop();
			}
		}
	} else {
		/*
		IX_NodeHeader* nheadL;
		IX_NodeHeader* nheadLL;
		PF_PageHandle* par;
		IX_Entry* enpar;
		IX_NodeHeader* nhead;
		int size = stack.size();
		for (int i = 0; i < size; ++i) {
			par = stack.top().tpage;
			enpar = stack.top().tentry;
			nhead = getNodeHead(par);

			if (checkLeaf(L))
				updateMBR<IX_Object>(enpar->mbr, L);
			else
				updateMBR<IX_Entry>(enpar->mbr, L);

			IX_Entry tmpen;
			if (checkLeaf(LL))
				updateMBR<IX_Object>(tmpen->mbr, LL);
			else
				updateMBR<IX_Entry>(tmpen->mbr, LL);

			insertObject
			delete L;
			L = par;
			stack.pop();

		}
		*/
	}
}


template<class T>
void IX_IndexHandle::updateMBR(IX_MBR& mbr, PF_PageHandle* L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	T* obj = (T*)((char*) nhead + sizeof(IX_NodeHeader));
	float lx, ly, hx, hy;
	ly = lx = FLT_MAX;
	hy = hx = FLT_MIN;
	for (int i = 0; i < nhead->nodeNum; ++i) {
		lx = min(lx, obj[i].mbr.lx);
		ly = min(ly, obj[i].mbr.ly);
		hx = max(hx, obj[i].mbr.hx);
		hy = max(hy, obj[i].mbr.hy);
	}
	mbr.lx = lx;
	mbr.ly = ly;
	mbr.hx = hx;
	mbr.hy = hy;
	return;
}


RC IX_IndexHandle::DeleteEntry(void* pData, const RID & rid) {
	// Implement this
}


RC IX_IndexHandle::ForcePages() {
	// Implement this
}

/*
void IX_IndexHandle::updateMBRByObject(MBR & mbr, PF_PageHandle* L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	IX_Object* obj = (IX_Object*)((char*) nhead + sizeof(IX_NodeHeader));
	float lx, ly, hx, hy;
	ly = lx = FLT_MAX;
	hy = hx = FLT_MIN;
	for (int i = 0; i < nhead->nodeNum; ++i) {
		lx = min(lx, obj[i].point.x);
		ly = min(ly, obj[i].point.y);
		hx = max(hx, obj[i].point.x);
		hy = max(hy, obj[i].point.y);
	}
	mbr.lx = lx;
	mbr.ly = ly;
	mbr.hx = hx;
	mbr.hy = hy;
	return;
}
*/