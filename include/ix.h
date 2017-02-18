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



enum IX_NodeType {
    ROOT = 1,
    LEAF = 2,
    NONLEAF = 4
};

struct IX_NodeHeader {
    int nodeNum;
    IX_NodeType type;
};

struct IX_Point {
    float x, y;
    bool operator==(const IX_Point& p) {
        return p.x == x && p.y == y;
    }

    void operator=(const IX_Point& p) {
        x = p.x;
        y = p.y;
    }
};

struct IX_MBR {
    float lx, ly;
    float hx, hy;

    bool in(const IX_Point& p) {
        return (p.x + 1e5F >= lx && p.y + 1e5F >= ly) && (p.x - 1e5F <= hx && p.y - 1e5F <= hy);
    }

    float enlargeTest(const IX_Point& p) {
        if (in(p)) return 0.0F;
        float clx = min(lx, p.x);
        float cly = min(ly, p.y);
        float chx = max(hx, p.x);
        float chy = max(hy, p.y);
        float carea = (chy - cly) * (chx - clx);
        float oarea = (hy - ly) * (hx - lx);
        return carea - oarea;
    }

};

struct IX_Entry {
    IX_MBR mbr;
    PageNum child;
};

struct IX_Object {
    IX_Point point;
    RID rid;
    IX_Object(IX_Point& pData, RID& id) {
        rid = id;
        point = pData;
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

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    private:
        int ENTRY_M;
        int ENTRY_m;
        int OBJECT_M;
        int OBJECT_m;
        PF_FileHandle* fileHandle;
        PF_PageHandle* fileHeader;
        PF_PageHandle* root;
        string* fileName;
        PF_FileHdr* hdr;
        stack<IX_Trace> trace;

        bool pageFull(PF_PageHandle* page);
        RC adjustTree(PF_PageHandle* L, PF_PageHandle* LL);
        RC insertObject(IX_Object* obj, PF_PageHandle* L);
        PF_PageHandle* splitNode(IX_Object* obj, PF_PageHandle* L);
        PF_PageHandle* chooseLeaf(IX_Object* obj, PF_PageHandle* root);

    public:
        IX_IndexHandle();
        ~IX_IndexHandle();

        // Insert a new index entry
        RC InsertEntry(void *pData, const RID &rid);

        // Delete a new index entry
        RC DeleteEntry(void *pData, const RID &rid);

        // Force index files to disk
        RC ForcePages();

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
