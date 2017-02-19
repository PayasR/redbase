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
	IX_Entry* obj = new IX_Entry(mbr, rid);
	PF_PageHandle* L = chooseLeaf(obj, root);
	PF_PageHandle* LL = NULL;
	insertObject(obj, L);
	if (pageFull(L))
		LL = splitNode(obj, L);
	adjustTree(L, LL);

	delete obj;
	delete L;
	if (LL != NULL)
		delete LL;
}


PF_PageHandle* IX_IndexHandle::chooseLeaf(IX_Entry* obj, PF_PageHandle* node, int& level) {
	if (checkLeaf(node) || level == getPageLevel(node))
		return node;
	else {
		PF_PageHandle* nextnode = findEnlargeLeast(obj, node);
		return chooseLeaf(obj, node);
	}
}

int IX_IndexHandle::getPageLevel(PF_PageHandle* node) {
	IX_NodeHeader* nhead = getNodeHead(node);
	return nhead->level;
}


PF_PageHandle* IX_IndexHandle::findEnlargeLeast(IX_Entry* obj, PF_PageHandle* node) {
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


IX_Entry* getDataList(IX_NodeHeader* nhead) {
	return (IX_Entry*)(nhead + 1);
}


RC IX_IndexHandle::insertObject(IX_Entry* obj, PF_PageHandle* L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	if (nhead != NULL) {
		IX_Entry* objList = getDataList(nhead);
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


PF_PageHandle* IX_IndexHandle::splitNode(IX_Entry* obj, PF_PageHandle* L) {
	IX_NodeHeader* nheadL = getNodeHead(L);
	IX_Entry* enList = getDataList(nheadL);
	if (nheadL->sortNeed) {
		sort(enList, enList + nheadL->nodeNum, sortMBR);
		nheadL->sortNeed = false;
	}

	// Initialize new page
	PF_PageHandle* LL = NULL;
	if (checkLeaf(L))
		LL = newPage(LEAF);
	else
		LL = newPage(NONLEAF);

	IX_NodeHeader* nheadLL = getNodeHead(LL);
	nheadLL->level = nheadL->level;

	// Copy data from L for split
	int remain = nheadL->nodeNum / 2;
	IX_Entry* enSpList = getDataList(nheadLL);
	nheadLL->nodeNum = nheadL->nodeNum - remain;
	memcpy(enSpList, enList + remain, sizeof(IX_Entry) * nheadLL->nodeNum);
	nheadL->nodeNum = remain;

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

			par = stack.top().tpage;
			enpar = stack.top().tentry;
			nhead = getNodeHead(par);

			if (nhead->enlargeNeed)
				updateMBR(enpar->mbr, L);

			delete L;
			L = par;
			stack.pop();
			return adjustTree(L, LL);
		}
		return 0;
	} else {
		int pn;
		IX_Entry* tmpen = new IX_Entry();
		if (!stack.empty()) {
			PF_PageHandle* par;
			IX_Entry* enpar;
			IX_NodeHeader* nhead;

			par = stack.top().tpage;
			enpar = stack.top().tentry;
			nhead = getNodeHead(par);

			updateMBR(enpar, L);
			updateMBR(tmpen, LL);

			LL->GetPageNum(pn);
			tmpen->rid = RID(pn, 0);
			insertObject(tmpen, par);

			delete L;
			delete LL;

			if (pageFull(par))
				LL = splitNode(par);
			else
				LL = NULL;
			L = par;
			stack.pop();
			return adjustTree(L, LL);
		} else {
			IX_NodeHeader* nheadL = getNodeHead(L);
			nheadL->type ^= ROOT;

			root = newPage(ROOT | NONLEAF);
			IX_NodeHeader* nhead = getNodeHead(root);
			nhead->level = nheadL->level + 1;

			// Insert L into root
			updateMBR(tmpen, L);
			L->GetPageNum(pn);
			tmpen->rid = RID(pn, 0);
			insertObject(tmpen, root);

			// Insert LL into root
			updateMBR(tmpen, LL);
			LL->GetPageNum(pn);
			tmpen->rid = RID(pn, 0);
			insertObject(tmpen, root);
			delete L;
			delete LL;
			return 0;
		}
	}
}


void IX_IndexHandle::updateMBR(IX_Entry* entry, PF_PageHandle* L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	IX_Entry* obj = getDataList(nhead);
	float lx, ly, hx, hy;
	ly = lx = FLT_MAX;
	hy = hx = FLT_MIN;
	for (int i = 0; i < nhead->nodeNum; ++i) {
		lx = min(lx, obj[i].mbr.lx);
		ly = min(ly, obj[i].mbr.ly);
		hx = max(hx, obj[i].mbr.hx);
		hy = max(hy, obj[i].mbr.hy);
	}
	entry->mbr.lx = lx;
	entry->mbr.ly = ly;
	entry->mbr.hx = hx;
	entry->mbr.hy = hy;
	nhead = getNodeHead(entry);
	nhead->enlargeNeed = false;
	nhead->sortNeed = true;
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