#include "BPlusTree.h"

#include <cstring>

// searches the relation specified using a B+ tree to find the next record that satisfies the specified condition. 
RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
	// declare searchIndex which will be used to store search index for attrName.
	IndexId searchIndex;

	/* get the search index corresponding to attribute with name attrName
	using AttrCacheTable::getSearchIndex(). */
	int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);   // puts into searchIndex and returns operation status
	if(ret != SUCCESS){
		return RecId{-1, -1};
	}

	AttrCatEntry attrCatEntry;
	ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	if(ret != SUCCESS){
	return RecId{-1, -1};
	}

	// declare variables block and index which will be used during search
	int block, index;

	if (searchIndex.block == -1 && searchIndex.index == -1) {
		// First search: start from root
		block = attrCatEntry.rootBlock;
		index = 0;

		// No B+ tree exists for this attribute
		if (block == -1) {
			return RecId{-1, -1};
		}
	} 
	else {
	// Continue from the entry after the previous hit
	block = searchIndex.block;
	index = searchIndex.index + 1;  // search is resumed from the next index.

	// load block into leaf using IndLeaf::IndLeaf().
	IndLeaf leaf(block);

	// declare leafHead which will be used to hold the header of leaf.
	HeadInfo leafHead;
	ret = leaf.getHeader(&leafHead);
	if(ret != SUCCESS){
		return RecId{-1, -1};
	}

	// need to go to the next block
	if (index >= leafHead.numEntries) {
		block = leafHead.rblock;
		index = 0;
		if (block == -1) 
		{
		// (end of linked list reached - the search is done.)
		return RecId{-1, -1};
		}
	}
	}

	/******  Traverse through all the internal nodes according to value
			 of attrVal and the operator op                             ******/

	/* (This section is only needed when
		- search restarts from the root block (when searchIndex is reset by caller)
		- root is not a leaf
		If there was a valid search index, then we are already at a leaf block
		and the test condition in the following loop will fail)
	*/

	while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) { 

		// load the block into internalBlk using IndInternal::IndInternal().
		IndInternal internalBlk(block);

		HeadInfo intHead;

		ret = internalBlk.getHeader(&intHead);
		if(ret != SUCCESS){
			return RecId{-1, -1};
		}

		// declare intEntry which will be used to store an entry of internalBlk.
		InternalEntry intEntry;

		if (op == NE || op == LT || op == LE) 
		{
			/*
			- NE: need to search the entire linked list of leaf indices of the B+ Tree,
			starting from the leftmost leaf index. Thus, always move to the left.

			- LT and LE: the attribute values are arranged in ascending order in the
			leaf indices of the B+ Tree. Values that satisfy these conditions, if
			any exist, will always be found in the left-most leaf index. Thus,
			always move to the left.
			*/

			internalBlk.getEntry(&intEntry, 0);
			block = intEntry.lChild;
		} 
		else {
			bool found = false;

   				 // Search for the appropriate child pointer
			for(int i = 0; i < intHead.numEntries; i++){
				internalBlk.getEntry(&intEntry, i);

				comparisonCount++;
				int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);

				// For EQ and GE, find the first key >= search value
				if((op == EQ || op == GE) && cmpVal >= 0){
					block = intEntry.lChild;
					found = true;
					break;
				}

				// For GT, find the first key > search value
				if(op == GT && cmpVal > 0){
					block = intEntry.lChild;
					found = true;
					break;
				}
			}

			if (!found) {
				// No suitable key found; follow the right child of the last entry
				internalBlk.getEntry(&intEntry, intHead.numEntries - 1);  // retrieves the last entry in the block
				block = intEntry.rChild;   //  will contain the block number of the right child of the entry
			}
		}
	}

	//STEP 3:  NOTE: `block` now has the block number of a leaf index block.
	/******  Identify the first leaf index entry from the current position
	 that satisfies our condition (moving right)             ******/

	 // TRAVERSING THRU THE LINKED LIST
	while (block != -1) {
		// load the block into leafBlk using IndLeaf::IndLeaf().
		IndLeaf leafBlk(block);
		HeadInfo leafHead;

		// load the header to leafHead using BlockBuffer::getHeader().
		ret = leafBlk.getHeader(&leafHead);
		if(ret != SUCCESS)
		{
			return RecId{-1, -1};
		}

		// declare leafEntry which will be used to store an entry from leafBlk
		Index leafEntry;

		while (index < leafHead.numEntries) 
		{
			ret = leafBlk.getEntry(&leafEntry, index);
			if(ret != SUCCESS)
			{
			return RecId{-1, -1};
			}
			
			comparisonCount++;
			int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
			if ( (op == EQ && cmpVal == 0) ||
			(op == LE && cmpVal <= 0) ||
			(op == LT && cmpVal < 0) ||
			(op == GT && cmpVal > 0) ||
			(op == GE && cmpVal >= 0) ||
			(op == NE && cmpVal != 0)
			) 

			{
			// (entry satisfying the condition found)
			IndexId newSearchIndex;
			newSearchIndex.block = block;
			newSearchIndex.index = index;

			ret = AttrCacheTable::setSearchIndex(relId, attrName, &newSearchIndex);
			if(ret != SUCCESS)
			{
				return RecId{-1, -1};
			}
			return RecId{leafEntry.block, leafEntry.slot};
			} 
			else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) 
			{
			/*future entries will not satisfy EQ, LE, LT since the values
			are arranged in ascending order in the leaves */
			return RecId{-1, -1};
			}

			// search next index.
			++index;
		}

		/*only for NE operation do we have to check the entire linked list;
		for all the other op it is guaranteed that the block being searched
		will have an entry, if it exists, satisying that op. */
		if (op != NE) 
		{
			break;
		}
		block = leafHead.rblock;
		index = 0;
	}

	// no entry satisying the op was found
	return RecId{-1, -1};
}


int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  if (attrCatEntry.rootBlock != -1) {
    return SUCCESS;  // index already exists
  }

  /****** Creating a new B+ Tree ******/
  IndLeaf rootBlockBuf;  // constructor 1: allocates a new leaf block
  int rootBlock = rootBlockBuf.getBlockNum();

  if (rootBlock == E_DISKFULL) {
    return E_DISKFULL;
  }

  // record the new root in the attribute cache (bPlusInsert reads it from there)
  attrCatEntry.rootBlock = rootBlock;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  int block = relCatEntry.firstBlk;

  /***** Traverse all record blocks and insert into the B+ tree *****/
  while (block != -1) {
    RecBuffer recBlock(block);

    unsigned char slotMap[relCatEntry.numSlotsPerBlk];
    recBlock.getSlotMap(slotMap);

    for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++) {
      if (slotMap[slot] == SLOT_OCCUPIED) {
        Attribute record[relCatEntry.numAttrs];
        recBlock.getRecord(record, slot);

        RecId recId{block, slot};

        int retVal = bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);
        if (retVal == E_DISKFULL) {
          return E_DISKFULL;  // bPlusInsert already destroyed the tree
        }
      }
    }

    HeadInfo header;
    recBlock.getHeader(&header);
    block = header.rblock;
  }

  return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
  if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  int type = StaticBuffer::getStaticBlockType(rootBlockNum);

  if (type == IND_LEAF) {
    IndLeaf leaf(rootBlockNum);
    leaf.releaseBlock();
    return SUCCESS;

  } else if (type == IND_INTERNAL) {
    IndInternal internalBlk(rootBlockNum);

    HeadInfo header;
    internalBlk.getHeader(&header);

    InternalEntry entry;

    // lChild of the first entry
    internalBlk.getEntry(&entry, 0);
    bPlusDestroy(entry.lChild);

    // rChild of every entry (rChild of entry i == lChild of entry i+1)
    for (int i = 0; i < header.numEntries; i++) {
      internalBlk.getEntry(&entry, i);
      bPlusDestroy(entry.rChild);
    }

    internalBlk.releaseBlock();
    return SUCCESS;

  } else {
    return E_INVALIDBLOCK;
  }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) {
    return ret;
  }

  int blockNum = attrCatEntry.rootBlock;
  if (blockNum == -1) {
    return E_NOINDEX;
  }

  int leafBlkNum = findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType);

  Index indexEntry;
  indexEntry.attrVal = attrVal;
  indexEntry.block = recId.block;
  indexEntry.slot = recId.slot;

  if (insertIntoLeaf(relId, attrName, leafBlkNum, indexEntry) == E_DISKFULL) {
    // destroy the whole tree and mark the attribute as having no index
    bPlusDestroy(blockNum);

    attrCatEntry.rootBlock = -1;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    return E_DISKFULL;
  }

  return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
  int blockNum = rootBlock;

  while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {
    IndInternal internalBlk(blockNum);

    HeadInfo header;
    internalBlk.getHeader(&header);

    // first entry whose attrVal >= attrVal
    InternalEntry entry;
    int index;
    for (index = 0; index < header.numEntries; index++) {
      internalBlk.getEntry(&entry, index);
      if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0) {
        break;
      }
    }

    if (index == header.numEntries) {
      // no such entry: go to rChild of the last entry
      internalBlk.getEntry(&entry, header.numEntries - 1);
      blockNum = entry.rChild;
    } else {
      blockNum = entry.lChild;
    }
  }

  return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  IndLeaf leaf(blockNum);

  HeadInfo blockHeader;
  leaf.getHeader(&blockHeader);

  Index indices[blockHeader.numEntries + 1];

  // copy existing entries + new entry, keeping ascending order
  int j = 0;
  bool inserted = false;
  for (int i = 0; i < blockHeader.numEntries; i++) {
    Index entry;
    leaf.getEntry(&entry, i);

    if (!inserted && compareAttrs(entry.attrVal, indexEntry.attrVal, attrCatEntry.attrType) > 0) {
      indices[j++] = indexEntry;
      inserted = true;
    }
    indices[j++] = entry;
  }
  if (!inserted) {
    indices[j++] = indexEntry;
  }

  if (blockHeader.numEntries != MAX_KEYS_LEAF) {
    // leaf has room
    blockHeader.numEntries++;
    leaf.setHeader(&blockHeader);

    for (int i = 0; i < blockHeader.numEntries; i++) {
      leaf.setEntry(&indices[i], i);
    }

    return SUCCESS;
  }

  // leaf is full: split
  int newRightBlk = splitLeaf(blockNum, indices);
  if (newRightBlk == E_DISKFULL) {
    return E_DISKFULL;
  }

  int ret;
  if (blockHeader.pblock != -1) {
    // not the root: push the middle value up into the parent
    InternalEntry internalEntry;
    internalEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
    internalEntry.lChild = blockNum;
    internalEntry.rChild = newRightBlk;

    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntry);
  } else {
    // root was split: make a new root
    ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
  }

  if (ret == E_DISKFULL) {
    return E_DISKFULL;
  }

  return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
  IndLeaf rightBlk;               // constructor 1: new block
  IndLeaf leftBlk(leafBlockNum);  // constructor 2: existing block

  int rightBlkNum = rightBlk.getBlockNum();
  int leftBlkNum = leftBlk.getBlockNum();

  if (rightBlkNum == E_DISKFULL) {
    return E_DISKFULL;
  }

  HeadInfo leftBlkHeader, rightBlkHeader;
  leftBlk.getHeader(&leftBlkHeader);
  rightBlk.getHeader(&rightBlkHeader);

  rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
  rightBlkHeader.pblock = leftBlkHeader.pblock;
  rightBlkHeader.lblock = leftBlkNum;
  rightBlkHeader.rblock = leftBlkHeader.rblock;
  rightBlk.setHeader(&rightBlkHeader);

  leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
  leftBlkHeader.rblock = rightBlkNum;
  leftBlk.setHeader(&leftBlkHeader);

  for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++) {
    leftBlk.setEntry(&indices[i], i);
    rightBlk.setEntry(&indices[i + (MAX_KEYS_LEAF + 1) / 2], i);
  }

  return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  IndInternal intBlk(intBlockNum);

  HeadInfo blockHeader;
  intBlk.getHeader(&blockHeader);

  InternalEntry internalEntries[blockHeader.numEntries + 1];

  // copy existing entries + new entry in ascending order
  int j = 0;
  int insertIdx = -1;
  for (int i = 0; i < blockHeader.numEntries; i++) {
    InternalEntry entry;
    intBlk.getEntry(&entry, i);

    // the new entry goes before the first entry whose key is >= its key
    // (that entry's lChild is the block that was just split)
    if (insertIdx == -1 && compareAttrs(entry.attrVal, intEntry.attrVal, attrCatEntry.attrType) >= 0) {
      insertIdx = j;
      internalEntries[j++] = intEntry;
    }
    internalEntries[j++] = entry;
  }
  if (insertIdx == -1) {
    insertIdx = j;
    internalEntries[j++] = intEntry;
  }

  // the entry right after the new one must now have the new rChild as its lChild
  if (insertIdx + 1 < blockHeader.numEntries + 1) {
    internalEntries[insertIdx + 1].lChild = intEntry.rChild;
  }

  if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
    blockHeader.numEntries++;
    intBlk.setHeader(&blockHeader);

    for (int i = 0; i < blockHeader.numEntries; i++) {
      intBlk.setEntry(&internalEntries[i], i);
    }

    return SUCCESS;
  }

  // block is full: split
  int newRightBlk = splitInternal(intBlockNum, internalEntries);

  if (newRightBlk == E_DISKFULL) {
    // free the not-yet-attached right subtree
    bPlusDestroy(intEntry.rChild);
    return E_DISKFULL;
  }

  int ret;
  if (blockHeader.pblock != -1) {
    InternalEntry middleEntry;
    middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
    middleEntry.lChild = intBlockNum;
    middleEntry.rChild = newRightBlk;

    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
  } else {
    ret = createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
  }

  if (ret == E_DISKFULL) {
    return E_DISKFULL;
  }

  return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
  IndInternal rightBlk;                 // constructor 1: new block
  IndInternal leftBlk(intBlockNum);     // constructor 2: existing block

  int rightBlkNum = rightBlk.getBlockNum();
  int leftBlkNum = leftBlk.getBlockNum();

  if (rightBlkNum == E_DISKFULL) {
    return E_DISKFULL;
  }

  HeadInfo leftBlkHeader, rightBlkHeader;
  leftBlk.getHeader(&leftBlkHeader);
  rightBlk.getHeader(&rightBlkHeader);

  rightBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;
  rightBlkHeader.pblock = leftBlkHeader.pblock;
  rightBlk.setHeader(&rightBlkHeader);

  leftBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;
  leftBlk.setHeader(&leftBlkHeader);

  // left  <- entries [0, 49]
  // right <- entries [51, 100]   (entry 50 moves up to the parent)
  int half = MAX_KEYS_INTERNAL / 2;
  for (int i = 0; i < half; i++) {
    leftBlk.setEntry(&internalEntries[i], i);
    rightBlk.setEntry(&internalEntries[i + half + 1], i);
  }

  // every child of the new right block must point back to it
  // children = lChild of entry 51, then rChild of entries 51..100
  auto setParent = [&](int childBlkNum) {
    BlockBuffer childBlk(childBlkNum);  // constructor 2
    HeadInfo childHeader;
    childBlk.getHeader(&childHeader);
    childHeader.pblock = rightBlkNum;
    childBlk.setHeader(&childHeader);
  };

  setParent(internalEntries[half + 1].lChild);
  for (int i = half + 1; i <= MAX_KEYS_INTERNAL; i++) {
    setParent(internalEntries[i].rChild);
  }

  return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  IndInternal newRootBlk;  // allocates a new internal block
  int newRootBlkNum = newRootBlk.getBlockNum();

  if (newRootBlkNum == E_DISKFULL) {
    // free the not-yet-attached right subtree
    bPlusDestroy(rChild);
    return E_DISKFULL;
  }

  HeadInfo newRootHeader;
  newRootBlk.getHeader(&newRootHeader);
  newRootHeader.numEntries = 1;
  newRootBlk.setHeader(&newRootHeader);

  InternalEntry rootEntry;
  rootEntry.lChild = lChild;
  rootEntry.attrVal = attrVal;
  rootEntry.rChild = rChild;
  newRootBlk.setEntry(&rootEntry, 0);

  // both children now have the new root as parent
  BlockBuffer lBlk(lChild);
  BlockBuffer rBlk(rChild);

  HeadInfo lHeader, rHeader;
  lBlk.getHeader(&lHeader);
  rBlk.getHeader(&rHeader);

  lHeader.pblock = newRootBlkNum;
  rHeader.pblock = newRootBlkNum;

  lBlk.setHeader(&lHeader);
  rBlk.setHeader(&rHeader);

  // point the attribute cache at the new root
  attrCatEntry.rootBlock = newRootBlkNum;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  return SUCCESS;
}
