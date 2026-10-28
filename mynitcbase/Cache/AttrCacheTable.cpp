#include "AttrCacheTable.h"
#include <cstring>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

// STAGE 3
// getAttrCatEntry - searches the cached linked list for the requested attribute and returns its AttrCatEntry.
int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf) { // (0,3,attrCatBuf) - gets attr no.3 of relation with rel-id=0 and puts into *attrCatBuf

    // Check relId
    if (relId < 0 || relId >= MAX_OPEN){  // must be 0-11
        return E_OUTOFBOUND;
    }

    // Check whether relation is open
    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    // Traverse linked list
    for ( AttrCacheEntry *entry = attrCache[relId];  entry != nullptr;   entry = entry->next ){

        if (entry->attrCatEntry.offset == attrOffset){
            // Copy the attribute catalog entry
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;         // Attribute with this offset doesn't exist
}


// getAttrCatEntry - searches the cached linked list for the requested attribute and returns its AttrCatEntry.
int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf) { // (0,3,attrCatBuf) - gets attr no.3 of relation with rel-id=0 and puts into *attrCatBuf
    if (relId < 0 || relId >= MAX_OPEN){  //// Check relId - must be 0-11
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {          // Check whether relation is open
        return E_RELNOTOPEN;
    }

    // Traverse linked list
    for ( AttrCacheEntry *entry = attrCache[relId];  entry != nullptr;   entry = entry->next ){

        // Check attribute name
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            *attrCatBuf = entry->attrCatEntry;      // Copy matching entry
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;         // Attribute was not found
}



// recordToAttrCatEntry - converts a catalog record Attribute[] form into the attrCatEntry[] form
void AttrCacheTable::recordToAttrCatEntry( union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry *attrCatEntry) {

    strcpy( attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal );
    strcpy( attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal );
    attrCatEntry->attrType =  (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->primaryFlag = (bool)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
    attrCatEntry->offset =  (int)record[ATTRCAT_OFFSET_INDEX].nVal;
}


// STAGE 10: Get search index by attribute name
int AttrCacheTable::getSearchIndex(
    int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {

    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    for (AttrCacheEntry *curr = attrCache[relId];
         curr != nullptr; curr = curr->next) {

        if (strcmp(curr->attrCatEntry.attrName, attrName) == 0) {
            *searchIndex = curr->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}


// STAGE 10: Get search index by attribute offset
int AttrCacheTable::getSearchIndex(
    int relId, int attrOffset, IndexId *searchIndex) {

    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    for (AttrCacheEntry *curr = attrCache[relId];
         curr != nullptr; curr = curr->next) {

        if (curr->attrCatEntry.offset == attrOffset) {
            *searchIndex = curr->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}


// STAGE 10: Set search index by attribute name
int AttrCacheTable::setSearchIndex(
    int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {

    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    for (AttrCacheEntry *curr = attrCache[relId];
         curr != nullptr; curr = curr->next) {

        if (strcmp(curr->attrCatEntry.attrName, attrName) == 0) {
            curr->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}


// STAGE 10: Set search index by attribute offset
int AttrCacheTable::setSearchIndex(
    int relId, int attrOffset, IndexId *searchIndex) {

    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    for (AttrCacheEntry *curr = attrCache[relId];
         curr != nullptr; curr = curr->next) {

        if (curr->attrCatEntry.offset == attrOffset) {
            curr->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}


// STAGE 10: Reset search index by attribute name
int AttrCacheTable::resetSearchIndex(
    int relId, char attrName[ATTR_SIZE]) {

    IndexId searchIndex = {-1, -1};
    return setSearchIndex(relId, attrName, &searchIndex);
}


// STAGE 10: Reset search index by attribute offset
int AttrCacheTable::resetSearchIndex(
    int relId, int attrOffset) {

    IndexId searchIndex = {-1, -1};
    return setSearchIndex(relId, attrOffset, &searchIndex);
}

// STAGE 11
int AttrCacheTable::setAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
    if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
      // copy the buffer into the cache entry and mark it dirty
      memcpy(&(entry->attrCatEntry), attrCatBuf, sizeof(AttrCatEntry));
      entry->dirty = true;
      return SUCCESS;
    }
  }

  return E_ATTRNOTEXIST;
}

int AttrCacheTable::setAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf) {
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
    if (entry->attrCatEntry.offset == attrOffset) {
      memcpy(&(entry->attrCatEntry), attrCatBuf, sizeof(AttrCatEntry));
      entry->dirty = true;
      return SUCCESS;
    }
  }

  return E_ATTRNOTEXIST;
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry, Attribute record[ATTRCAT_NO_ATTRS]) {
  strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
  strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);
  record[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrCatEntry->attrType;
  record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = attrCatEntry->primaryFlag;
  record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = attrCatEntry->rootBlock;
  record[ATTRCAT_OFFSET_INDEX].nVal = attrCatEntry->offset;
}
