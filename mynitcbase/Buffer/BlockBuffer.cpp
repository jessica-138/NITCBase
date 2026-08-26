#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// CONSTRUCTOR
BlockBuffer::BlockBuffer(int blockNum){ 
    this->blockNum = blockNum;
}

// since RecBuffer represents a record-block, it calles BlockBuffer constructor to store block number
RecBuffer::RecBuffer(int blockNum) : BlockBuffer(blockNum){}

// loadBlockAndGetBufferPtr
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    if (bufferNum == E_BLOCKNOTINBUFFER) {
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
        
        if(bufferNum < 0){
            return bufferNum;
        }

        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }
    // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
    *buffPtr = StaticBuffer::blocks[bufferNum];
    return SUCCESS;
}

// getHeader 
// loads the block header into the pointer 'head'
int BlockBuffer::getHeader(struct HeadInfo *head){
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS){
        return ret;
    }

    // copy each header field from buffer into head(the HeadInfo struture)
    memcpy(&head->blockType, bufferPtr+0, 4);
    memcpy(&head->pblock, bufferPtr+4, 4);
    memcpy(&head->lblock, bufferPtr+8, 4);
    memcpy(&head->rblock, bufferPtr+12, 4);
    memcpy(&head->numEntries, bufferPtr+16, 4);
    memcpy(&head->numAttrs, bufferPtr+20, 4);
    memcpy(&head->numSlots, bufferPtr+24, 4);
    memcpy(&head->reserved, bufferPtr+28, 4);

    return SUCCESS;
}

// getRecord
// loads the record at slotNum into the pointer 'rec'
int RecBuffer::getRecord(union Attribute *rec, int slotNum){
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS){
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);  // this.. the class already stores the blockNum. using that we get the header of the corresponding block

    // Check if valid slotNum
    int attrCount = head.numAttrs;
    int slotCount = head.numSlots; // number of slots
    
    if(slotNum < 0 || slotNum >= slotCount){
        return E_OUTOFBOUND;
    }

    // each entry in slotmap tells us whether the slot is occupied or not
    unsigned char *slotMap = bufferPtr + HEADER_SIZE;
    if(slotMap[slotNum] == SLOT_UNOCCUPIED){
        return E_FREESLOT;
    }

    int recordSize = attrCount * ATTR_SIZE;

    // our pointer = buf + 32 + L + (L * 16K)
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (slotNum * recordSize);
    memcpy(rec, slotPointer, recordSize);     // COPIES RECORD FROM BUFFER TO OUTPUT ARRAY
    return SUCCESS;
}



// setRecord - STAGE 2 - exercise 2 - Writes the record 'rec' into slot number 'slotNum'
int RecBuffer::setRecord(union Attribute *rec, int slotNum) {

    // Read the header
    HeadInfo head;
    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    // Check whether slot number is valid
    if (slotNum < 0 || slotNum >= slotCount)
        return E_OUTOFBOUND;

    // Read the entire block into memory
    unsigned char buffer[BLOCK_SIZE];
    Disk::readBlock(buffer, this->blockNum);

    // Calculate size of one record
    int recordSize = attrCount * ATTR_SIZE;

    // Calculate where this record starts
    unsigned char *slotPointer =
        buffer + HEADER_SIZE + slotCount + (slotNum * recordSize);

    // Copy the new record into the block
    memcpy(slotPointer, rec, recordSize);

    // Mark the slot as occupied
    unsigned char *slotMap = buffer + HEADER_SIZE;
    slotMap[slotNum] = SLOT_OCCUPIED;

    // Write the modified block back to disk
    Disk::writeBlock(buffer, this->blockNum);

    return SUCCESS;
}



// getSlotMap
int RecBuffer::getSlotMap(unsigned char * slotMap){
    unsigned char *bufferPtr;

    // Get block from buffer
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS){
        return ret;
    }

    // Get blk header
    HeadInfo head;
    ret = getHeader(&head);
    if(ret != SUCCESS){
        return ret;
    }

    // slotmap starts immediately after header
    int slotCount = head.numSlots;
    unsigned char *slotMapInBuff = bufferPtr + HEADER_SIZE;
    memcpy(slotMap, slotMapInBuff, slotCount);      // Copy slotmap to caller's array (slotMap)
    return SUCCESS;
}

 // compareAttrs(recordValue, searchValue, NUMBER) and we can determine whether =, !=, <, <=, >, >= is satisfied
int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {
    double diff;

    if (attrType == STRING) {
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else {            // attribute type is NUMBER
        diff = attr1.nVal - attr2.nVal;
    }

    // compare the difference
    if (diff > 0)
        return 1;

    if (diff < 0)
        return -1;

    return 0;
}