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
    int ret = RelCacheTable::getSearchIndex(relId, &prevRecId);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    int block;
    int slot;

    // If search index is {-1,-1}, start from first block
    if (prevRecId.block == -1 && prevRecId.slot == -1) {
        RelCatEntry relCatEntry;
        ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);  //get relation information from relation cache

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


    // Search through all blocks
    while (block != -1) {
        RecBuffer recBuffer(block);        // Create buffer for current block

        // Get block header
        HeadInfo header;
        ret = recBuffer.getHeader(&header);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // Get slot map
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
