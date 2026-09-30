#include "StaticBuffer.h"

// define blocks and metainfo (arrays)
unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

// constructor
StaticBuffer::StaticBuffer() {
    // Load the 4 block allocation map blocks from disk
    // 4 blocks because -- 4 × 2048 = 8192 bytes

    for (int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; i++) {
        Disk::readBlock(
            blockAllocMap + (i * BLOCK_SIZE),
            i
        );
    }

    // Initialize all buffer metadata
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        metainfo[i].free = true;
        metainfo[i].dirty = false;
        metainfo[i].timeStamp = -1;
        metainfo[i].blockNum = -1;
    }
}

// destructor
StaticBuffer::~StaticBuffer() {

    // Write the block allocation map back to disk
    for (int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; i++) {

        Disk::writeBlock(
            blockAllocMap + (i * BLOCK_SIZE),
            i
        );
    }

    // Write all dirty buffers back to disk
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (!metainfo[i].free && metainfo[i].dirty) {

            Disk::writeBlock(
                blocks[i],
                metainfo[i].blockNum
            );
        }
    }
}

// getFreeBuffer 
// looks for an empty buffer slot and allocates given blocknumber to that buffer .. returns index of the buffer number that stores the REQUESTED BLOCK
// if no free buffer, USE LRU  - largest timestamp = least recently used

int StaticBuffer::getFreeBuffer(int blockNum) {

    // Check whether the block number is valid
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Increase age of every occupied buffer
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (!metainfo[i].free) {
            metainfo[i].timeStamp++;
        }
    }

    int bufferNum = -1;

    // First look for a completely free buffer
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (metainfo[i].free) {
            bufferNum = i;
            break;
        }
    }

    // If no free buffer exists, use LRU
    if (bufferNum == -1) {
        int maxTimeStamp = -1;

        // Find the buffer with the largest timestamp
        for (int i = 0; i < BUFFER_CAPACITY; i++) {
            if (metainfo[i].timeStamp > maxTimeStamp) {

                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }

        // Write the old block back if it was modified
        if (metainfo[bufferNum].dirty) {

            Disk::writeBlock(
                blocks[bufferNum],
                metainfo[bufferNum].blockNum
            );
        }
    }


    // Assign this buffer to the new block
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    return bufferNum;
}


// getBufferNum - returns index of the buffer that stores the block(given, blockNum)
int StaticBuffer::getBufferNum(int blockNum){
    if (blockNum < 0 || blockNum >= DISK_BLOCKS){
        return E_OUTOFBOUND;
    }

    for(int i=0; i< BUFFER_CAPACITY; i++){
        if(!metainfo[i].free && metainfo[i].blockNum == blockNum){
            return i;
        }
    }

    return E_BLOCKNOTINBUFFER;
}


//setDirtyBit()
int StaticBuffer::setDirtyBit(int blockNum) {

    // Find the buffer containing this block
    int bufferNum = getBufferNum(blockNum);

    // Block is outside valid range
    if (bufferNum == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }

    // Block is not currently in the buffer
    if (bufferNum == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }

    // Mark the buffer as modified
    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}
