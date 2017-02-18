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
	OBJECT_M = (PF_PAGE_SIZE - sizeof(IX_Header)) / sizeof(IX_Object) - 1;
	OBJECT_m = OBJECT_M / 2;
}

IX_IndexHandle::~IX_IndexHandle() {

}

RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid) {
	IX_Point* point = (IX_Point*) pData;
	IX_Object* obj = new IX_Object(*point, rid);
	PF_PageHandle* L = chooseLeaf(obj, root);
	PF_PageHandle* LL = NULL;
	insertObject(obj, L);
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


RC IX_IndexHandle::insertObject(IX_Object* obj, PF_PageHandle* L) {
	IX_NodeHeader* head = getNodeHead(L);
	if (head != NULL) {
		head->sortNeed = true;
		IX_Object* list = (IX_Object*) ((char*) head + sizeof(IX_NodeHeader));
		list[head->nodeNum++] = obj;
		int pn;
		int err;
		if (!(err = L->GetPageNum(pn)))
			err = fileHandle->MarkDirty(pn);
		return err;
	}
	return PF_INVALIDPAGE;
}


bool IX_IndexHandle::pageFull(PF_PageHandle* page) {
	char* pdata;
	IX_NodeHeader* head;
	RC err;
	if (!(err = page->GetData(pdata))) {
		head = (IX_NodeHeader*) pdata;
		if (head->type & LEAF) {
			if (head->nodeNum > ENTRY_M)
				return true;
			else
				return false;
		} else {
			if (head->nodeNum > OBJECT_M)
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
	IX_NodeHeader* nhead;
	if (nhead = getNodeHead(node)) {
		if (nhead & LEAF) return true;
		else return false;
	}
	cout << "[IX_IndexHandle::checkLeaf] Get Page Header Error" << endl;
	return false;
}


PF_PageHandle* IX_IndexHandle::findEnlargeLeast(IX_Object* obj, PF_PageHandle* node) {

	IX_NodeHeader* nhead;
	if (nhead = getNodeHead(node)) {
		if (nhead->type & LEAF)
			return node;
		else {
			int nodeNum = nhead->nodeNum;
			IX_Entry* en = (IX_Entry*)((char*) nhead + sizeof(IX_NodeHeader));
			IX_Entry* minen;
			float minarea = FLT_MAX;
			for (int i = 0; i < nodeNum; ++i, ++en) {
				float diffarea = en->mbr.enlargeTest(obj->point);
				if (diffarea < minarea) {
					minarea = diffarea;
					minen = en;
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

// Need Template
PF_PageHandle* IX_IndexHandle::splitNode(IX_Object* obj, PF_PageHandle* L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	IX_Object* en = (IX_Object*)((char*) nhead + sizeof(IX_NodeHeader));
	if (nhead->sortNeed) {
		sort(en, en + nhead->nodeNum, sortObject);
		nhead->sortNeed = false;
	}

	int remain = nhead->nodeNum / 2;
	PF_PageHandle* LL = new PF_PageHandle();

	if (!fileHandle->AllocatePage(*LL)) {
		IX_NodeHeader* nheadll;
		if (nheadll = getNodeHead(LL)) {
			// Initialize new page
			int pn;
			nheadll->nodeNum = nhead->nodeNum - remain;
			nheadll->sortNeed = false;
			nheadll->enlargeNeed = false;
			if (nhead->type & LEAF)
				nheadll->type = LEAF;
			else
				nheadll->type = NONLEAF;

			LL->GetPageNum(pn);
			fileHandle->MarkDirty(pn);

			IX_Object* enll = (IX_Object*)((char*) nheadll + sizeof(IX_NodeHeader));
			memcpy(enll, en + remain, sizeof(IX_Object) * nheadll->nodeNum);
			fileHeader->GetPageNum(pn);
			fileHandle->MarkDirty(pn);
			++hdr->numPages;
		}
	}
	return LL;
}

RC IX_IndexHandle::adjustTree(PF_PageHandle * L, PF_PageHandle * LL) {
	if (LL == NULL) {
		// Only first update is based on Object
		PF_PageHandle* par = stack.top().tpage;
		IX_Entry* enpar = stack.top().tentry;
		IX_NodeHeader* nhead = getNodeHead(par);
		if (nhead->enlargeNeed) {
			updateMBRByObject(enpar->mbr, L);
			nhead->sortNeed = true;
		}
		stack.pop();
		delete L;
		L = par;

		// Update based on Entry
		int size = stack.size();
		for (int i = 0; i < size; ++i) {
			par = stack.top().tpage;
			enpar = stack.top().tentry;
			nhead = getNodeHead(par);
			if (nhead->enlargeNeed) {
				updateMBRByEntry(enpar->mbr, L);
				nhead->sortNeed = true;
			}
			delete L;
			L = par;
			stack.pop();
		}
	} else {
		// implement
	}
}

void IX_IndexHandle::updateMBRByObject(MBR & mbr, PF_PageHandle * L) {
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

void IX_IndexHandle::updateMBRByEntry(MBR & mbr, PF_PageHandle * L) {
	IX_NodeHeader* nhead = getNodeHead(L);
	IX_Entry* obj = (IX_Entry*)((char*) nhead + sizeof(IX_NodeHeader));
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


RC IX_IndexHandle::DeleteEntry(void *pData, const RID & rid) {
	// Implement this
}

RC IX_IndexHandle::ForcePages() {
	// Implement this
}
