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


// INSERT (BLOCK ACCESS LEVEL)
int BlockAccess::insert(int relId, Attribute *record) {

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


    // Get relation information from the cache
    RelCatEntry relCatEntry;

    int ret = RelCacheTable::getRelCatEntry(
        relId,
        &relCatEntry
    );

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

        // Access the existing record block
        RecBuffer block(blockNum);

        // Get its header
        HeadInfo head;

        ret = block.getHeader(&head);

        if (ret != SUCCESS) {
            return ret;
        }

        // Get its slot map
        unsigned char slotMap[numOfSlots];

        ret = block.getSlotMap(slotMap);

        if (ret != SUCCESS) {
            return ret;
        }


        // Look for a free slot
        for (int i = 0; i < numOfSlots; i++) {

            if (slotMap[i] == SLOT_UNOCCUPIED) {

                // Found where the record can be inserted
                rec_id.block = blockNum;
                rec_id.slot = i;

                break;
            }
        }


        // Free slot found → stop searching
        if (rec_id.block != -1) {
            break;
        }


        // No free slot → move to next block
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }


    /*
     * No free slot was found.
     * Therefore, allocate a new record block.
     */
    if (rec_id.block == -1) {

        // RELCAT cannot allocate more blocks
        if (relId == RELCAT_RELID) {
            return E_MAXRELATIONS;
        }


        // Allocate a new record block
        RecBuffer newBlock;

        // Get the newly allocated block number
        int newBlockNum = newBlock.getBlockNum();

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


        /*
         * Link the new block into the relation's
         * record-block linked list.
         */
        if (prevBlockNum != -1) {

            // There was an existing last block
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
        else {

            // No previous block → this is first block
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
    ret = block.setRecord(
        record,
        rec_id.slot
    );

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

    // One more record is now stored in this block
    head.numEntries++;

    ret = block.setHeader(&head);

    if (ret != SUCCESS) {
        return ret;
    }


    // One more record exists in the relation
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