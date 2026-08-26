#include "RelCacheTable.h"
#include <cstring>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];

// getRelCatEntry  -- relation info stored at relId is put into relCatBuf  -- CACHE ->CALLER
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf) {
    if (relId < 0 || relId >= MAX_OPEN) {   // is relId valid?
        return E_OUTOFBOUND;
    }
    if (relCache[relId] == nullptr) {      // is there a relation stored there?
        return E_RELNOTOPEN;
    }

     // copy the value to the relCatBuf argument
    *relCatBuf = relCache[relId]->relCatEntry;
    return SUCCESS;
}


// recordToRelCatEntry -- converts a record from Attribute[] format to RelCatEntry format
/* Converts a relation catalog record to RelCatEntry struct
    We get the record as Attribute[] from the BlockBuffer.getRecord() function.
    This function will convert that to a struct RelCatEntry type.
NOTE: this function expects the caller to allocate memory for `*relCatEntry`
*/
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry* relCatEntry){
    strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
    relCatEntry->numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
    relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
    relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
    relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}


// getSearchIndex - current search index of the relation -- is copied into  RecId* searchIndex
int RelCacheTable::getSearchIndex(int relId, RecId* searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {    // checks whether relid is valid
        return E_OUTOFBOUND;
    }

    if (relCache[relId] == nullptr) {      // check if relation is open(i.e. , if its present in the cache)
        return E_RELNOTOPEN;
    }

    *searchIndex = relCache[relId]->searchIndex;  // copy search index from cache to output(which is, RecId* searchIndex)
    return SUCCESS;
}


// setSearchIndex
// stores the new searchIndex for the relation
int RelCacheTable::setSearchIndex(int relId, RecId* searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {         // checks if relation_id is valid
        return E_OUTOFBOUND;
    }

    if (relCache[relId] == nullptr) {            // checks if relation is present in the cache
        return E_RELNOTOPEN;
    }

    // update the cached searchIndex - copy the given searchIndex into the relation's cache
    relCache[relId]->searchIndex = *searchIndex;
    return SUCCESS;
}


//resetSearchIndex
int RelCacheTable::resetSearchIndex(int relId){
    if (relId < 0 || relId >= MAX_OPEN) {   // check if rel_id is valid
        return E_OUTOFBOUND;
    }
    if (relCache[relId] == nullptr) {      // checks if relation is present in the cache
        return E_RELNOTOPEN;
    }

    // No current search position
    relCache[relId]->searchIndex.block = -1;
    relCache[relId]->searchIndex.slot = -1;

    return SUCCESS;
}