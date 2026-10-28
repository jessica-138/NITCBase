#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

// constructor
OpenRelTable::OpenRelTable() {

    // initialise relCache and attrCache with nullptr
    for (int i = 0; i < MAX_OPEN; ++i) {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;

        tableMetaInfo[i].free = true;
    }

    tableMetaInfo[RELCAT_RELID].free = false;
    strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

    tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);


    // *********   setting up Relation Catalog relation in the Relation Cache Table

    RecBuffer relCatBlock(RELCAT_BLOCK);      // block 4
    Attribute relCatRecord[RELCAT_NO_ATTRS];


    //  RELATIONCAT    -----  setting up RELATIONCAT relation at rel-id 0 (of relation cache)

    relCatBlock.getRecord(                    // getting record stored at slot 0 of Relation Catalog Block (i.e. the relaion catalog RELATION)
        relCatRecord,
        RELCAT_SLOTNUM_FOR_RELCAT  // 0
    );

    RelCacheEntry relCacheEntry;           // (structure to store into relation cache)

    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );

    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    relCacheEntry.dirty = false;
    relCacheEntry.searchIndex.block = -1;
    relCacheEntry.searchIndex.slot = -1;

    RelCacheTable::relCache[RELCAT_RELID] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;


    //   ATTRIBUTECAT   -- caching the ATTRIBUTECAT relation at rel-id 1

    relCatBlock.getRecord(                      // getting record stored at slot 1 of Relation Catalog Block
        relCatRecord,
        RELCAT_SLOTNUM_FOR_ATTRCAT
    );

    RelCacheEntry attrCatCacheEntry;

    RelCacheTable::recordToRelCatEntry(        // record to RelCatEntry
        relCatRecord,
        &attrCatCacheEntry.relCatEntry
    );

    attrCatCacheEntry.recId.block = RELCAT_BLOCK;
    attrCatCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    attrCatCacheEntry.dirty = false;
    attrCatCacheEntry.searchIndex.block = -1;
    attrCatCacheEntry.searchIndex.slot = -1;

    RelCacheTable::relCache[ATTRCAT_RELID] =
        (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

    *(RelCacheTable::relCache[ATTRCAT_RELID]) =
        attrCatCacheEntry;



    //  ******************** ATTRIBUTE CACHE *******************

    RecBuffer attrCatBlock(ATTRCAT_BLOCK);        // to access block 5
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];    // temporary array used to read ONE attribute-catalog record.


    // ---------- ATTRIBUTES OF RELATIONCAT ----------

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *tail = nullptr;

    for (int i = 0; i < RELCAT_NO_ATTRS; i++) {

        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );

        AttrCacheEntry *newEntry =
            (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = i;

        newEntry->dirty = false;
        newEntry->next = nullptr;

        if (head == nullptr) {
            head = newEntry;
            tail = newEntry;

        }
        else {
            tail->next = newEntry;
            tail = newEntry;
        }
    }

    AttrCacheTable::attrCache[RELCAT_RELID] = head;


    // ---------- ATTRIBUTES OF ATTRIBUTECAT ----------

    head = nullptr;
    tail = nullptr;

    for ( int i = RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i++) {
        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );

        AttrCacheEntry *newEntry =
            (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = i;

        newEntry->dirty = false;
        newEntry->next = nullptr;


        if (head == nullptr) {
            head = newEntry;
            tail = newEntry;

        }
        else {
            tail->next = newEntry;
            tail = newEntry;
        }
    }

    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

}

// destructor
// Destroys the Open Relation Table and writes dirty catalog cache entries back to disk.
OpenRelTable::~OpenRelTable() {

    // Close all user relations
    for (int i = 2; i < MAX_OPEN; i++) {
        if (!tableMetaInfo[i].free) {
            closeRel(i);
        }
    }

     // Write back RELATIONCAT cache entry if dirty.
    if (RelCacheTable::relCache[RELCAT_RELID] != nullptr && RelCacheTable::relCache[RELCAT_RELID]->dirty) {
        // convert the cached RELATIONCAT metadata into a record and update RELATIONCAT's catalog entry on disk.
        RelCacheEntry *entry = RelCacheTable::relCache[RELCAT_RELID];
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&entry->relCatEntry, record);
        RecBuffer relCatBlock(entry->recId.block);
        relCatBlock.setRecord(record, entry->recId.slot);
    }

    // Write back ATTRIBUTECAT cache entry if dirty.
    if (RelCacheTable::relCache[ATTRCAT_RELID] != nullptr && RelCacheTable::relCache[ATTRCAT_RELID]->dirty) {
        // convert the cached ATTRIBUTECAT metadata into a record and update ATTRIBUTECAT's catalog entry in RELATIONCAT on disk.   
        RelCacheEntry *entry = RelCacheTable::relCache[ATTRCAT_RELID];   // cached metadata for ATTRCAT relation
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&entry->relCatEntry, record);
        RecBuffer relCatBlock(entry->recId.block);
        relCatBlock.setRecord(record, entry->recId.slot);
    }


    // Free catalog relation-cache entries (manually) - 
    //(why?) because closeRel() cannot free RELCAT and ATTRCAT caches during normal DB operations (closeRel() has a condn that does not allow CLOSING of relcache and attrcache)
    free(RelCacheTable::relCache[ATTRCAT_RELID]);
    free(RelCacheTable::relCache[RELCAT_RELID]);
    RelCacheTable::relCache[ATTRCAT_RELID] = nullptr;
    RelCacheTable::relCache[RELCAT_RELID] = nullptr;

    // Free catalog attribute-cache linked lists
    for (int i = 0; i < 2; i++) {
        AttrCacheEntry *current = AttrCacheTable::attrCache[i];
        while (current != nullptr) {
            AttrCacheEntry *next = current->next;
            free(current);
            current = next;
        }

        AttrCacheTable::attrCache[i] = nullptr;
    }
}

// return relId of relation, with given name relName
int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
    for(int i=0; i<MAX_OPEN; i++){   // check every possible relation slot

        if (!tableMetaInfo[i].free) {      // check occupied entries only
            if (strcmp(tableMetaInfo[i].relName, relName) == 0) {    // compare relation names 
                return i;
            }
        }

    }
    return E_RELNOTOPEN;
}


// returns REL-ID of any FREE slots in the CACHE
// getFreeOpenRelTableEntry()
int OpenRelTable::getFreeOpenRelTableEntry() {
    for (int i = 2; i < MAX_OPEN; i++) {
        if (tableMetaInfo[i].free) {
            return i;
        }
    }

    return E_CACHEFULL;
}

// openRel()
int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
     // Check if already open
    if (getRelId(relName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Find free cache slot
    int relId = getFreeOpenRelTableEntry();
    if (relId == E_CACHEFULL) {
        return E_CACHEFULL;
    }

    // Search RELATIONCAT for the relation
    Attribute attrVal;
    strcpy(attrVal.sVal, relName);

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);  // RELCAT_ATTR_RELNAME = "RelName" - In RELATIONCAT, search the RelName attribute.
    RecId recId = BlockAccess::linearSearch(  // search Relation 0 (RELATIONCAT) where RelName = relName (like "Students")
        RELCAT_RELID,
        relCatAttrName,
        attrVal,
        EQ
    );

    if (recId.block == INVALID_BLOCKNUM) {
        return E_RELNOTEXIST;
    }

    // Read RELATIONCAT record
    RecBuffer relCatBlock(recId.block);    // loads the block with blocknumber recId.block into relCatBlock
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int ret = relCatBlock.getRecord(relCatRecord, recId.slot);  // loads record at recId.slot into relCatRecord
    if (ret != SUCCESS) {
        return ret;
    }

    // Create and fill relation cache entry
    RelCacheEntry *relEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

    RelCacheTable::recordToRelCatEntry(  // convert the caltalog record form (Attribute[]) into RelCatEntry form
        relCatRecord,
        &relEntry->relCatEntry
    );

    relEntry->recId = recId;
    relEntry->dirty = false;
    relEntry->searchIndex.block = INVALID_BLOCKNUM;
    relEntry->searchIndex.slot = INVALID_BLOCKNUM;

    RelCacheTable::relCache[relId] = relEntry;   // put the relEntry into the relation cache

    // We want to convert from Attribute[] to AttrCacheEntry* form 
    // Build attribute cache linked list
    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *tail = nullptr;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);  // resets to {-1,-1}

    while (true) {
         // Search ATTRIBUTECAT for the next attribute of this relation
        char attrCatAttrName[ATTR_SIZE];
        strcpy(attrCatAttrName, ATTRCAT_ATTR_RELNAME);  // attrCatAttrName = "RelName"

         // Search ATTRIBUTECAT where RelName = "Students"
        RecId attrRecId = BlockAccess::linearSearch( 
            ATTRCAT_RELID,
            attrCatAttrName,    // AtrrName we re searching on - RelName
            attrVal,            // attrVal = "Students" in Attribute[] form
            EQ
        );

        if (attrRecId.block == INVALID_BLOCKNUM) {  // no more attributes found
            break;
        }

        // block and slot of the RECORD we found thru linearSearch
        RecBuffer attrBlock(attrRecId.block);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];

        ret = attrBlock.getRecord(attrRecord, attrRecId.slot);   // put the record in slot=attrRecId.slot AND block=attrBlock into attrRecord
        if (ret != SUCCESS) {
            return ret;
        }

        AttrCacheEntry *newEntry =(AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));  // create one node of the LL (EACH NODE IS OF type AttrCacheEntry*)

        AttrCacheTable::recordToAttrCatEntry(  //converts a catalog record Attribute[] form into the attrCatEntry[] form
            attrRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId = attrRecId;
        newEntry->dirty = false;
        newEntry->next = nullptr;

        if (head == nullptr) {
            head = newEntry;
            tail = newEntry;
        }
        else {
            tail->next = newEntry;
            tail = newEntry;
        }
    }

    AttrCacheTable::attrCache[relId] = head;   // store into attrCache

    // Mark slot as occupied
    tableMetaInfo[relId].free = false;
    strcpy(tableMetaInfo[relId].relName, relName);

    return relId;
}


// closeRel()
int OpenRelTable::closeRel(int relId) {

    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
        return E_NOTPERMITTED;
    }

    if (tableMetaInfo[relId].free) {  // not open - so nothing to close
        return E_RELNOTOPEN;
    }

    // BEFORE FREEING RELATION CACHE, CHECK
    // Check whether relation cache was modified
    if (RelCacheTable::relCache[relId] != nullptr && RelCacheTable::relCache[relId]->dirty) {
        RelCacheEntry *relEntry =
            RelCacheTable::relCache[relId];

        // Convert cache entry back to a record
        Attribute record[RELCAT_NO_ATTRS];

        RelCacheTable::relCatEntryToRecord(
            &relEntry->relCatEntry,
            record
        );

        // Write updated record back to RELCAT buffer
        RecBuffer relCatBlock(relEntry->recId.block);

        int ret = relCatBlock.setRecord(
            record,
            relEntry->recId.slot
        );

        if (ret != SUCCESS)
            return ret;
    }

    // free the relation cache entry
    if (RelCacheTable::relCache[relId] != nullptr) {
        free(RelCacheTable::relCache[relId]);
        RelCacheTable::relCache[relId] = nullptr;
    }

    // Traverse the LL of attribute cache entries, write back dirty ones, then free
    AttrCacheEntry *current = AttrCacheTable::attrCache[relId];

    while (current != nullptr) {
        AttrCacheEntry *next = current->next;

        // NEW: write back the attribute catalog entry if it was modified
        if (current->dirty) {
            Attribute attrRecord[ATTRCAT_NO_ATTRS];

            AttrCacheTable::attrCatEntryToRecord(
                &current->attrCatEntry,
                attrRecord
            );

            RecBuffer attrCatBlock(current->recId.block);

            int ret = attrCatBlock.setRecord(
                attrRecord,
                current->recId.slot
            );

            if (ret != SUCCESS)
                return ret;
        }

        free(current);
        current = next;
    }

    AttrCacheTable::attrCache[relId] = nullptr;

    // Mark slot as free
    tableMetaInfo[relId].free = true;

    return SUCCESS;
}