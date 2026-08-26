#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

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

    // for Exercise 1
    tableMetaInfo[2].free = false;
    strcpy(tableMetaInfo[2].relName, "Students");


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

    RelCacheTable::relCache[RELCAT_RELID] = 
                                (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

    

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

    // ---------- STUDENTS RELATION ----------

    int studentsRelId = 2;
    int studentsSlot = -1;

    HeadInfo relCatHeader;
    relCatBlock.getHeader(&relCatHeader);

    // Find Students in Relation Catalog
    for (int i = 0; i < relCatHeader.numEntries; i++) {

        relCatBlock.getRecord(relCatRecord, i);

        if (strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,
                "Students") == 0) {

            studentsSlot = i;
            break;
        }
    }

    if (studentsSlot != -1) {

        // Create Students relation-cache entry
        RelCacheEntry studentsEntry;

        RelCacheTable::recordToRelCatEntry(
            relCatRecord,
            &studentsEntry.relCatEntry
        );

        studentsEntry.recId.block = RELCAT_BLOCK;
        studentsEntry.recId.slot = studentsSlot;
        studentsEntry.dirty = false;

        // Initialize search index
        studentsEntry.searchIndex.block = -1;
        studentsEntry.searchIndex.slot = -1;

        RelCacheTable::relCache[studentsRelId] =
            (RelCacheEntry*)malloc(sizeof(RelCacheEntry));

        *(RelCacheTable::relCache[studentsRelId]) = studentsEntry;


        // Create Students attribute-cache linked list
        AttrCacheEntry *studentsHead = nullptr;
        AttrCacheEntry *studentsTail = nullptr;

        HeadInfo attrCatHeader;
        attrCatBlock.getHeader(&attrCatHeader);

        for (int i = 0; i < attrCatHeader.numEntries; i++) {

            attrCatBlock.getRecord(attrCatRecord, i);

            if (strcmp(
                    attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                    "Students"
                ) == 0) {

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

                if (studentsHead == nullptr) {
                    studentsHead = newEntry;
                    studentsTail = newEntry;
                }
                else {
                    studentsTail->next = newEntry;
                    studentsTail = newEntry;
                }
            }
        }

        AttrCacheTable::attrCache[studentsRelId] = studentsHead;
    }
}


OpenRelTable::~OpenRelTable() {

    // Free relation cache entries
    for (int i = 0; i < MAX_OPEN; i++) {
        if (RelCacheTable::relCache[i] != nullptr) {
            free(RelCacheTable::relCache[i]);
            RelCacheTable::relCache[i] = nullptr;
        }
    }

    // Free attribute cache linked lists
    for (int i = 0; i < MAX_OPEN; i++) {
        AttrCacheEntry *current =
            AttrCacheTable::attrCache[i];

        while (current != nullptr) {
            AttrCacheEntry *next = current->next;
            free(current);
            current = next;
        }

        AttrCacheTable::attrCache[i] = nullptr;
    }
}

// return relId of relation with name relName
int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
    // check every possible relation slot
    for(int i=0; i<MAX_OPEN; i++){

        if (!tableMetaInfo[i].free) {      // check occupied entries only

            if (strcmp(tableMetaInfo[i].relName, relName) == 0) {    // compare relation names 
                return i;
            }

        }
    }
    return E_RELNOTOPEN;
}