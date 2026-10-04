#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// CONSTRUCTOR -  access an already-existing block
BlockBuffer::BlockBuffer(int blockNum){ 
    this->blockNum = blockNum;
}

// constructor - allocate a new block
BlockBuffer::BlockBuffer(char blockType) {
    int type;
    // Convert character to block type
    if (blockType == 'R')
        type = REC;
    else if (blockType == 'I')
        type = IND_INTERNAL;
    else if (blockType == 'L')
        type = IND_LEAF;

    // Allocate a free block
    this->blockNum = getFreeBlock(type);
}

// getBlockNum()
int BlockBuffer::getBlockNum() {
    return this->blockNum;
}


// getFreeBlock()
int BlockBuffer::getFreeBlock(int blockType) {
    int blockNum = -1;

    // Search for an unused disk block
    for (int i = 0; i < DISK_BLOCKS; i++) {
        if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK) {
            blockNum = i;
            break;
        }
    }

    // No free block
    if (blockNum == -1)
        return E_DISKFULL;

    // Store block number in this object
    this->blockNum = blockNum;

    // Allocate a buffer for the block
    int bufferNum =
        StaticBuffer::getFreeBuffer(blockNum);

    if (bufferNum < 0)
        return bufferNum;

    // Initialize the block header
    struct HeadInfo head;

    head.blockType = blockType;
    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;

    // Write header into buffer
    int ret = setHeader(&head);

    if (ret != SUCCESS)
        return ret;

    // Update block type and allocation map
    ret = setBlockType(blockType);

    if (ret != SUCCESS)
        return ret;

    return blockNum;
}


// setHeader()
int BlockBuffer::setHeader(struct HeadInfo *head) {

    unsigned char *bufferPtr;

    // Get buffer containing this block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    // Treat beginning of block as header
    struct HeadInfo *bufferHeader =
        (struct HeadInfo *)bufferPtr;

    // Copy header information
    bufferHeader->blockType = head->blockType;
    bufferHeader->pblock = head->pblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->rblock = head->rblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numSlots = head->numSlots;

    // Mark block as modified
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

//setBlockType()
int BlockBuffer::setBlockType(int blockType) {

    unsigned char *bufferPtr;

    // Get buffer containing this block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    // Store block type in first 4 bytes
    *((int32_t *)bufferPtr) = blockType;

    // Update block allocation map
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    // Mark block as modified
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}


// since RecBuffer represents a record-block, it calles BlockBuffer constructor to store block number
RecBuffer::RecBuffer()
    : BlockBuffer('R') {}

RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {}


// setSlotMap()
int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    // Get buffer containing this block
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    // Get header to know number of slots
    struct HeadInfo head;

    ret = getHeader(&head);

    if (ret != SUCCESS)
        return ret;

    // Copy slot map into block
    memcpy(bufferPtr + HEADER_SIZE,
           slotMap,
           head.numSlots);

    // Mark block as modified
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}


// loadBlockAndGetBufferPtr
// separate these two cases: 
//          already present -> update timestamps; absent -> call getFreeBuffer()
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {

    // Check whether the block is already in the buffer
    int bufferNum = StaticBuffer::getBufferNum(
        this->blockNum
    );

    // Block is already present
    if (bufferNum >= 0) {

        // This block was just used, so reset its timestamp
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;

        // Increase timestamp of all other occupied buffers
        for (int i = 0; i < BUFFER_CAPACITY; i++) {
            if (i != bufferNum && !StaticBuffer::metainfo[i].free) {
                StaticBuffer::metainfo[i].timeStamp++;
            }
        }

    }

    // Block is not in the buffer
    else if (bufferNum == E_BLOCKNOTINBUFFER) {

        // Get a free/LRU buffer
        bufferNum = StaticBuffer::getFreeBuffer( this->blockNum );

        if (bufferNum < 0) {
            return bufferNum;
        }

        // Load block from disk into the selected buffer
        int ret = Disk::readBlock( StaticBuffer::blocks[bufferNum],
                                   this->blockNum
        );

        if (ret != SUCCESS) {
            return ret;
        }
    }

    // Invalid block number
    else {
        return bufferNum;
    }

    // Return pointer to the buffer containing the block
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
// STAGE 6 -  Loads the block into the buffer, finds the requested record's location,
//            copies the new record there, and marks the block dirty instead of immediately writing it to disk.

int RecBuffer::setRecord( union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;

    // Load the block into the buffer
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    // Get block information
    HeadInfo head;
    ret = getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;


    // Check slot number
    if (slotNum < 0 || slotNum >= slotCount) {
        return E_OUTOFBOUND;
    }

    // Calculate size of one record
    int recordSize = attrCount * ATTR_SIZE;

    // Locate the required record in the buffer
    unsigned char *slotPointer =
        bufferPtr +
        HEADER_SIZE +
        slotCount +
        (slotNum * recordSize);

    // Copy new record into the buffer
    memcpy(slotPointer, rec, recordSize);

    // Mark the block as modified
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    if (ret != SUCCESS) {
        return ret;
    }
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


// releaseBlock() 
// Releases the current block from the BUFFER and marks its disk block as unused in BLOCKALLOCMAP
void BlockBuffer::releaseBlock() {

    // Nothing to release (ALRDY RELEASED)
    if (this->blockNum == INVALID_BLOCKNUM) {
        return;
    }

    // Check whether this block is currently in the buffer.
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    // Free the buffer entry if the block is loaded.
    if (bufferNum >= 0) {
        StaticBuffer::metainfo[bufferNum].free = true;
        StaticBuffer::metainfo[bufferNum].dirty = false;
    }

    // update BLOCK ALLOC MAP - Mark the disk block as unused.
    StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;

    this->blockNum = INVALID_BLOCKNUM;   // This object no longer refers to a valid block.
}