#include "StaticBuffer.h"

// define blocks and metainfo (arrays)
unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

// constructor
StaticBuffer::StaticBuffer(){
    for(int i=0; i < BUFFER_CAPACITY; i++){
        metainfo[i].free = true;
    }
}

// destructor
StaticBuffer::~StaticBuffer() {  // write-back code written here
}

// getFreeBuffer 
// looks for an empty buffer slot and allocates given blocknumber to that buffer .. returns index of the buffer number that stores the REQUESTED BLOCK
int StaticBuffer::getFreeBuffer(int blockNum) {
    if (blockNum < 0 || blockNum >= DISK_BLOCKS){  // valid block number = 0 to 8191
        return E_OUTOFBOUND;
    }

    int allocatedBuffer = -1;
    
    for(int i=0; i < BUFFER_CAPACITY; i++){
        if(metainfo[i].free){
            allocatedBuffer = i;
            break;
        }
    }
    if(allocatedBuffer == -1){
        return E_OUTOFBOUND;
    }

    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].blockNum = blockNum;

    return allocatedBuffer;    // returns -1, if not found
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