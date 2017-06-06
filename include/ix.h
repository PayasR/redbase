//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"
#include <string>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <statck>

const int ROOT = 1;
const int LEAF = 2;
const int NONLEAF = 4;


int sortMBR(IX_Entry& a, IX_Entry& b) {
	return a.mbr.lx < b.mbr.lx;
}

struct IX_NodeHeader {
	int nodeNum;
	int type;
	int level;
	bool sortNeed;
	bool enlargeNeed;
};


struct IX_MBR {
	float lx, ly;
	float hx, hy;

	bool in(const IX_Point& p) {
		return (p.x + 1e5F >= lx && p.y + 1e5F >= ly) && (p.x - 1e5F <= hx && p.y - 1e5F <= hy);
	}

	float enlargeTest(const IX_MBR& p) {
		if (in(p)) return 0.0F;
		float clx = min(lx, p.lx);
		float cly = min(ly, p.ly);
		float chx = max(hx, p.hx);
		float chy = max(hy, p.hy);
		float carea = (chy - cly) * (chx - clx);
		float oarea = (hy - ly) * (hx - lx);
		return carea - oarea;
	}

	void operator=(const IX_MBR& p) {
		lx = p.lx;
		ly = p.ly;
		hx = p.hx;
		hy = p.hy;
	}
};

struct IX_Entry {
	IX_MBR mbr;
	RID rid;
	IX_Entry(IX_MBR* pData, RID& id) {
		rid = id;
		point.hx = point.lx = pData->lx;
		point.hy = point.ly = pData->ly;
	}
};

struct IX_Trace {
	PF_PageHandle* tpage;
	IX_Entry* tentry;
	IX_Trace(PF_PageHandle* page, IX_Entry* entry) {
		tpage = page;
		tentry = entry;
	}
}

struct IX_ReinEntry {
	IX_Entry entry;
	int level;
}

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
	private:
		int ENTRY_M;
		int ENTRY_m;
		PF_FileHandle* fileHandle;
		PF_PageHandle* fileHeaderPage; // First page in file which stores location of root and pages of files
		PF_FileHdr* fileHeader;  // File header for first page

		PF_PageHandle* root;  // Root node
		string* fileName;
		stack<IX_Trace> trace;

		bool pageFull(PF_PageHandle* page);
		RC adjustTree(PF_PageHandle* L, PF_PageHandle* LL);
		PF_PageHandle* chooseLeaf(IX_Entry* obj, PF_PageHandle* root);
		PF_PageHandle* newPage(int& type);
		RC markPageDirty(PF_PageHandle* page);
		IX_NodeHeader* getNodeHead(PF_PageHandle* node);
		PF_PageHandle* findEnlargeLeast(IX_Entry* obj, PF_PageHandle* node);

		RC insertObject(IX_Entry* obj, PF_PageHandle* L);
		PF_PageHandle* splitNode(IX_Entry* obj, PF_PageHandle* L);
		void updateMBR(IX_Entry* entry, PF_PageHandle* L);
		IX_Entry* getDataList(IX_NodeHeader* nhead);
		int getPageLevel(PF_PageHandle* node);
	public:
		IX_IndexHandle();
		~IX_IndexHandle();

		// Insert a new index entry
		RC InsertEntry(void *pData, const RID &rid);

		// Delete a new index entry
		RC DeleteEntry(void *pData, const RID &rid);

		// Force index files to disk
		RC ForcePages();

		PF_PageHandle* newPage(IX_NodeType& type);
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
	public:
		IX_IndexScan();
		~IX_IndexScan();

		// Open index scan
		RC OpenScan(const IX_IndexHandle &indexHandle,
		            CompOp compOp,
		            void *value,
		            ClientHint  pinHint = NO_HINT);


		// Get the next matching entry return IX_EOF if no more matching
		// entries.
		RC GetNextEntry(RID &rid);

		// Close index scan
		RC CloseScan();
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
	private:
		PF_Manager* pageManager;
		const int INT_LENGTH = 11;
	public:
		IX_Manager(PF_Manager &pfm);
		~IX_Manager();

		// Create a new Index
		RC CreateIndex(const char *fileName, int indexNo,
		               AttrType attrType, int attrLength);

		// Destroy and Index
		RC DestroyIndex(const char *fileName, int indexNo);

		// Open an Index
		RC OpenIndex(const char *fileName, int indexNo,
		             IX_IndexHandle &indexHandle);

		// Close an Index
		RC CloseIndex(IX_IndexHandle &indexHandle);
};

//
// Print-error function
//
void IX_PrintError(RC rc);

#define IX_BADINDEXSPEC         (START_IX_WARN + 0) // Bad Specification for Index File
#define IX_BADINDEXNAME         (START_IX_WARN + 1) // Bad index name
#define IX_INVALIDINDEXHANDLE   (START_IX_WARN + 2) // FileHandle used is invalid
#define IX_INVALIDINDEXFILE     (START_IX_WARN + 3) // Bad index file
#define IX_NODEFULL             (START_IX_WARN + 4) // A node in the file is full
#define IX_BADFILENAME          (START_IX_WARN + 5) // Bad file name
#define IX_INVALIDBUCKET        (START_IX_WARN + 6) // Bucket trying to access is invalid
#define IX_DUPLICATEENTRY       (START_IX_WARN + 7) // Trying to enter a duplicate entry
#define IX_INVALIDSCAN          (START_IX_WARN + 8) // Invalid IX_Indexscsan
#define IX_INVALIDENTRY         (START_IX_WARN + 9) // Entry not in the index
#define IX_EOF                  (START_IX_WARN + 10)// End of index file
#define IX_LASTWARN             IX_EOF

#define IX_ERROR                (START_IX_ERR - 0) // error
#define IX_LASTERROR            IX_ERROR

#endif
