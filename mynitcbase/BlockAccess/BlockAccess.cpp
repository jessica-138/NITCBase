#include "BlockAccess.h"
#include <cstring>

RecId BlockAccess::linearSearch(
    int relId,
    char attrName[ATTR_SIZE],
    Attribute attrVal,
    int op
){
    // Get previous search position from relation cache (using getSearchIndex)
    RecId prevRecId;
    int ret = RelCacheTable::getSearchIndex(relId, &prevRecId);  // current search index of the relation -- is copied into  RecId* searchIndex
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    int block;
    int slot;

    // If search index is {-1,-1}, (means it is a new search), start from first block and first slot
    if (prevRecId.block == -1 && prevRecId.slot == -1) {
         //get relation information from relation cache
        RelCatEntry relCatEntry;
        ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry); 
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        block = relCatEntry.firstBlk;
        slot = 0;
    }

    else {                       // Continue from the slot after previous result
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }


    // Search through all blocks until there are no more blocks
    while (block != -1) {
        RecBuffer recBuffer(block);        // Create a RecBuffer for current block

        // Get block header of curr block
        HeadInfo header;
        ret = recBuffer.getHeader(&header);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // Get slot map of current block
        unsigned char slotMap[header.numSlots];
        ret = recBuffer.getSlotMap(slotMap);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }


        // if all slots in current block are finished, move to the next block
        if (slot >= header.numSlots) {
            block = header.rblock;
            slot = 0;
            continue;
        }


        // if current slot is empty, skip it
        if (slotMap[slot] == SLOT_UNOCCUPIED) {
            slot++;
            continue;
        }

        // get the current record (at slotnumber 'slot')
        Attribute record[header.numAttrs];
        ret = recBuffer.getRecord(record, slot);   // loads record at slotnumber 'slot' into 'record'
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // Find the attribute offset using attr cache
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(
            relId,
            attrName,
            &attrCatEntry
        );

        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // compare the record's attribute with attrVal
        int cmpVal = compareAttrs(  record[attrCatEntry.offset],  attrVal,   attrCatEntry.attrType  );

        // Check whether condition is satisfied
        bool condition = false;

        if (
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0) ||
            (op == LE && cmpVal <= 0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0) ||
            (op == GE && cmpVal >= 0)
        ) {
            condition = true;
        }


        // If condition is satisfied
        if (condition) {

            RecId currentRecId{block, slot};

            // Update search index
            ret = RelCacheTable::setSearchIndex(  relId,   &currentRecId  );  // sets searchindex for relation at relId 

            if (ret != SUCCESS) {
                return RecId{-1, -1};
            }

            // Return the matching record's RecId
            return currentRecId;
        }

        // Move to next slot
        slot++;
    }

    return RecId{-1, -1};            // No matching record found
}



// stage 6 implementation

// renameRelation
int BlockAccess::renameRelation(
    char oldName[ATTR_SIZE],
    char newName[ATTR_SIZE]
) {
    // Search RELATIONCAT for the new name
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;
    strcpy(newRelationName.sVal, newName);

    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);

    RecId recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relCatAttrName,
        newRelationName,
        EQ
    );

    // New relation name already exists
    if (recId.block != INVALID_BLOCKNUM) {
        return E_RELEXIST;
    }


    // Search RELATIONCAT for the old relation name
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;
    strcpy(oldRelationName.sVal, oldName);

    recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relCatAttrName,
        oldRelationName,
        EQ
    );

    // Old relation does not exist
    if (recId.block == INVALID_BLOCKNUM) {
        return E_RELNOTEXIST;
    }


    // Read the relation catalog record
    RecBuffer relCatBlock(recId.block);

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int ret = relCatBlock.getRecord(
        relCatRecord,
        recId.slot
    );

    if (ret != SUCCESS) {
        return ret;
    }


    // Save number of attributes before changing the record
    int numAttrs =
        (int)relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    // Change relation name
    strcpy(
        relCatRecord[RELCAT_REL_NAME_INDEX].sVal,
        newName
    );


    // Write modified relation catalog record back to buffer
    ret = relCatBlock.setRecord(
        relCatRecord,
        recId.slot
    );

    if (ret != SUCCESS) {
        return ret;
    }


    // Update all corresponding ATTRIBUTECAT entries
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    for (int i = 0; i < numAttrs; i++) {

        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            relCatAttrName,
            oldRelationName,
            EQ
        );

        if (attrRecId.block == INVALID_BLOCKNUM) {
            break;
        }

        RecBuffer attrBlock(attrRecId.block);

        Attribute attrRecord[ATTRCAT_NO_ATTRS];

        ret = attrBlock.getRecord(
            attrRecord,
            attrRecId.slot
        );

        if (ret != SUCCESS) {
            return ret;
        }

        // Change relation name in attribute catalog
        strcpy(
            attrRecord[ATTRCAT_REL_NAME_INDEX].sVal,
            newName
        );

        // Write modified attribute catalog record
        ret = attrBlock.setRecord(
            attrRecord,
            attrRecId.slot
        );

        if (ret != SUCCESS) {
            return ret;
        }
    }

    return SUCCESS;
}



//renameAttribute()
int BlockAccess::renameAttribute(
    char relName[ATTR_SIZE],
    char oldName[ATTR_SIZE],
    char newName[ATTR_SIZE]
) {
    // Check whether the relation exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);

    RecId relRecId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relCatAttrName,
        relNameAttr,
        EQ
    );

    if (relRecId.block == INVALID_BLOCKNUM) {
        return E_RELNOTEXIST;
    }


    // Search all attributes belonging to the relation
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    RecId attrToRenameRecId{-1, -1};

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    while (true) {

        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            relCatAttrName,
            relNameAttr,
            EQ
        );

        // No more attributes
        if (attrRecId.block == INVALID_BLOCKNUM) {
            break;
        }

        RecBuffer attrBlock(attrRecId.block);

        int ret = attrBlock.getRecord(
            attrCatRecord,
            attrRecId.slot
        );

        if (ret != SUCCESS) {
            return ret;
        }


        // Check whether the new attribute name already exists
        if (strcmp(
                attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                newName
            ) == 0) {
            return E_ATTREXIST;
        }


        // Remember the record containing the old attribute
        if (strcmp(
                attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                oldName
            ) == 0) {

            attrToRenameRecId = attrRecId;
        }
    }


    // Old attribute was not found
    if (attrToRenameRecId.block == INVALID_BLOCKNUM) {
        return E_ATTRNOTEXIST;
    }


    // Read the attribute catalog record to modify
    RecBuffer attrBlock(attrToRenameRecId.block);

    int ret = attrBlock.getRecord(
        attrCatRecord,
        attrToRenameRecId.slot
    );

    if (ret != SUCCESS) {
        return ret;
    }


    // Change attribute name
    strcpy(
        attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
        newName
    );


    // Write modified record back to buffer
    ret = attrBlock.setRecord(
        attrCatRecord,
        attrToRenameRecId.slot
    );

    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}


    /*
     * Insert a record into the relation.
     *
     * 1. Get relation information from the relation cache.
     * 2. Traverse its record blocks looking for a free slot.
     * 3. If no free slot exists, allocate a new record block.
     * 4. Insert the record into the selected slot.
     * 5. Mark that slot as occupied.
     * 6. Increment the block's number of entries.
     * 7. Increment the relation's number of records.
     */


// INSERT (BLOCK ACCESS LEVEL)
int BlockAccess::insert(int relId, Attribute *record) {

    // Get relation information from the cache
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry( relId, &relCatEntry );
    if (ret != SUCCESS) {
        return ret;
    }

    // Start from the first record block
    int blockNum = relCatEntry.firstBlk;

    // Will store the location where record is inserted
    RecId rec_id = {-1, -1};

    // Information needed for record blocks
    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    // Previous block in linked list
    int prevBlockNum = -1;

    /*
     * Search through the existing record blocks
     * until we find a free slot.
     */
    while (blockNum != -1) {
        RecBuffer block(blockNum);   // Access the existing record block
        HeadInfo head;              // Get its header

        ret = block.getHeader(&head);
        if (ret != SUCCESS) {
            return ret;
        }

        unsigned char slotMap[numOfSlots];    // Get its slot map
        ret = block.getSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        // look for a free slot (by looking through the slot map)
        for (int i = 0; i < numOfSlots; i++) {
            if (slotMap[i] == SLOT_UNOCCUPIED) {  // Found where the record can be inserted
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }

        // Free slot found => stop searching and BREAK
        if (rec_id.block != -1) {
            break;
        }

        // No free slot => move to next block (while loop continues)
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }


    /* IF NO free slot was found.
    SO, allocate a NEW record BLOCK. 
    */
    if (rec_id.block == -1) {
        // RELCAT cannot allocate more blocks
        if (relId == RELCAT_RELID) {
            return E_MAXRELATIONS;
        }

        RecBuffer newBlock;       // Allocate a new record block
        int newBlockNum = newBlock.getBlockNum();      // Get the newly allocated block number
        if (newBlockNum == E_DISKFULL) {
            return E_DISKFULL;
        }
        if (newBlockNum < 0) {
            return newBlockNum;
        }

        // Record will go into slot 0 of new block
        rec_id.block = newBlockNum;
        rec_id.slot = 0;

        // Set up the new block's header
        HeadInfo head;

        head.blockType = REC;
        head.pblock = -1;
        head.lblock = prevBlockNum;
        head.rblock = -1;
        head.numEntries = 0;
        head.numAttrs = numOfAttributes;
        head.numSlots = numOfSlots;

        ret = newBlock.setHeader(&head);

        if (ret != SUCCESS) {
            return ret;
        }


        // Initially every slot is free
        unsigned char slotMap[numOfSlots];

        for (int i = 0; i < numOfSlots; i++) {
            slotMap[i] = SLOT_UNOCCUPIED;
        }

        ret = newBlock.setSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }


        /* Link the new block into the relation's
         * record-block linked list.
         */
        if (prevBlockNum != -1) {   // There was an existing last block
            RecBuffer prevBlock(prevBlockNum);  
            ret = prevBlock.getHeader(&head);

            if (ret != SUCCESS) {
                return ret;
            }

            // Previous block now points to new block
            head.rblock = rec_id.block;
            ret = prevBlock.setHeader(&head);

            if (ret != SUCCESS) {
                return ret;
            }
        }
        else {      // PREVbLOCK = -1 --- No previous block => this is first block
            relCatEntry.firstBlk = rec_id.block;

            ret = RelCacheTable::setRelCatEntry(
                relId,
                &relCatEntry
            );

            if (ret != SUCCESS) {
                return ret;
            }
        }


        // New block is always the last block
        relCatEntry.lastBlk = rec_id.block;

        ret = RelCacheTable::setRelCatEntry(
            relId,
            &relCatEntry
        );

        if (ret != SUCCESS) {
            return ret;
        }
    }


    /*
     * We now know exactly where the record goes.
     */

    // Access the block containing the selected slot
    RecBuffer block(rec_id.block);

    // Store the record
    ret = block.setRecord(record, rec_id.slot);

    if (ret != SUCCESS) {
        return ret;
    }


    // Get the slot map
    unsigned char slotMap[numOfSlots];
    ret = block.getSlotMap(slotMap);
    if (ret != SUCCESS) {
        return ret;
    }

    // Mark the inserted slot as occupied
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    ret = block.setSlotMap(slotMap);
    if (ret != SUCCESS) {
        return ret;
    }

    // Get the block header
    HeadInfo head;
    ret = block.getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    // One more record is now stored in this block -- UPDATE HEADER
    head.numEntries++;
    ret = block.setHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    // One more record exists in the relation -- UPDATE RELCATALOG
    relCatEntry.numRecs++;
    ret = RelCacheTable::setRelCatEntry(
        relId,
        &relCatEntry
    );
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}


/* Finds the next record satisfying the condition and copies it
 * into the GIVEN record array.
 *
 * Stage 8 uses linearSearch because B+ tree indexes are not
 * implemented yet
 */
int BlockAccess::search( int relId, Attribute *record, 
    char attrName[ATTR_SIZE], Attribute attrVal, int op
) {
    // Find the next matching record.
    RecId recId = BlockAccess::linearSearch(
        relId,
        attrName,
        attrVal,
        op
    );

    // No matching record was found.
    if (recId.block == INVALID_BLOCKNUM) {
        return E_NOTFOUND;
    }

    // Read the matching record from its block.
    RecBuffer recBuffer(recId.block);

    int ret = recBuffer.getRecord(record, recId.slot);

    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}


// deleteRelation()
/*
 * Deletes a relation, releases its record blocks, removes its
 * catalog entries, and updates the catalog caches.
 */
int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {

    // Catalog relations cannot be deleted.
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Search RELATIONCAT for the relation.
    int ret = RelCacheTable::resetSearchIndex(RELCAT_RELID);
    if (ret != SUCCESS) {
        return ret;
    }

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);

    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrName, relNameAttr, EQ);

    // Relation does not exist.
    if (relCatRecId.block == INVALID_BLOCKNUM) {
        return E_RELNOTEXIST;
    }

    // Read the relation's RELATIONCAT record.
    RecBuffer relCatBlock(relCatRecId.block);
    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];

    ret = relCatBlock.getRecord(relCatEntryRecord, relCatRecId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    // Get the first record block of the relation.
    int firstBlock = (int)relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;

    // Get the number of attributes.
    int numAttrs = (int)relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    // --------------------------------------------------
    // 1. DELETE ALL RECORD BLOCKS OF THE RELATION
    // --------------------------------------------------

    int blockNum = firstBlock;
    while (blockNum != INVALID_BLOCKNUM) {

        RecBuffer block(blockNum);   // Access the current record block.

        HeadInfo header;            // Read its header before releasing it.
        ret = block.getHeader(&header);
        if (ret != SUCCESS) {
            return ret;
        }

        int nextBlock = header.rblock;    // Save the next block before releasing this one.

        // Release the current block.
        block.releaseBlock();

        // Continue with the next block.
        blockNum = nextBlock;
    }

    // --------------------------------------------------
    // 2. DELETE ATTRIBUTECAT ENTRIES
    // --------------------------------------------------

    ret = RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    if (ret != SUCCESS) {
        return ret;
    }

    int numberOfAttributesDeleted = 0;

    while (true) {
        
        // Find the next ATTRIBUTECAT entry for this relation.
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relCatAttrName, relNameAttr, EQ);

        // No more entries.
        if (attrCatRecId.block == INVALID_BLOCKNUM) {
            break;
        }

        numberOfAttributesDeleted++;

        // Access the ATTRIBUTECAT block.
        RecBuffer attrCatBlock(attrCatRecId.block);
        HeadInfo header;

        ret = attrCatBlock.getHeader(&header);
        if (ret != SUCCESS) {
            return ret;
        }

        // Read the ATTRIBUTECAT record.
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        ret = attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);
        if (ret != SUCCESS) {
            return ret;
        }

        // Stage 8 has no indexes yet.
        int rootBlock = (int)attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        // Keep the variable because it is part of the documented
        // deletion algorithm. B+ tree destruction comes later.
        (void)rootBlock;

        // Get the block's slot map.
        unsigned char slotMap[header.numSlots];

        ret = attrCatBlock.getSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        // Mark this ATTRIBUTECAT slot as free.
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;

        ret = attrCatBlock.setSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        // Decrease number of entries in this block.
        header.numEntries--;

        ret = attrCatBlock.setHeader(&header);
        if (ret != SUCCESS) {
            return ret;
        }

        // If the block became empty, remove it from the
        // ATTRIBUTECAT linked list and release it.
        if (header.numEntries == 0) {

            int leftBlock = header.lblock;
            int rightBlock = header.rblock;

            // Update the previous block.
            if (leftBlock != INVALID_BLOCKNUM) {

                RecBuffer left(leftBlock);
                HeadInfo leftHeader;

                ret = left.getHeader(&leftHeader);
                if (ret != SUCCESS) {
                    return ret;
                }

                leftHeader.rblock = rightBlock;

                ret = left.setHeader(&leftHeader);
                if (ret != SUCCESS) {
                    return ret;
                }
            }

            // Update the next block.
            if (rightBlock != INVALID_BLOCKNUM) {

                RecBuffer right(rightBlock);
                HeadInfo rightHeader;

                ret = right.getHeader(&rightHeader);
                if (ret != SUCCESS) {
                    return ret;
                }

                rightHeader.lblock = leftBlock;

                ret = right.setHeader(&rightHeader);
                if (ret != SUCCESS) {
                    return ret;
                }
            }
            else {

                // This was the last ATTRIBUTECAT block.
                RelCatEntry attrCatRelEntry;

                ret = RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatRelEntry);
                if (ret != SUCCESS) {
                    return ret;
                }

                attrCatRelEntry.lastBlk = leftBlock;

                ret = RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatRelEntry);
                if (ret != SUCCESS) {
                    return ret;
                }
            }

            // Release the empty block.
            attrCatBlock.releaseBlock();

            /*
             * The search index still refers to the released block.
             * Reset it so the next search starts from the beginning
             * of the ATTRIBUTECAT relation.
             */
            ret = RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
            if (ret != SUCCESS) {
                return ret;
            }
        }
    }

    // --------------------------------------------------
    // 3. DELETE RELATION'S RELATIONCAT ENTRY
    // --------------------------------------------------

    RecBuffer relCat(RELCAT_BLOCK);
    HeadInfo relCatHeader;

    ret = relCat.getHeader(&relCatHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    // One RELATIONCAT entry is being removed.
    relCatHeader.numEntries--;

    ret = relCat.setHeader(&relCatHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    // Get RELATIONCAT slot map.
    unsigned char relCatSlotMap[relCatHeader.numSlots];

    ret = relCat.getSlotMap(relCatSlotMap);
    if (ret != SUCCESS) {
        return ret;
    }

    // Mark the relation's catalog entry as free.
    relCatSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;

    ret = relCat.setSlotMap(relCatSlotMap);
    if (ret != SUCCESS) {
        return ret;
    }

    // --------------------------------------------------
    // 4. UPDATE CATALOG CACHE METADATA
    // --------------------------------------------------

    // Update RELATIONCAT's number of records.
    RelCatEntry relCatCacheEntry;

    ret = RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatCacheEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    relCatCacheEntry.numRecs--;

    ret = RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatCacheEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    // Update ATTRIBUTECAT's number of records.
    RelCatEntry attrCatCacheEntry;

    ret = RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatCacheEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    attrCatCacheEntry.numRecs -= numberOfAttributesDeleted;

    ret = RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatCacheEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}