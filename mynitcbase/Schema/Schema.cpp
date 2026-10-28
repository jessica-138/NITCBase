#include "Schema.h"

#include <cmath>
#include <cstring>

// openRel
int Schema::openRel(char relName[ATTR_SIZE]){
    int relId=OpenRelTable::openRel(relName);
    if(relId>=0){
        return SUCCESS;
    }
    return relId;
}


// closeRel
int Schema::closeRel(char relName[ATTR_SIZE]){
    // catalog relations cannot be closed
    if(strcmp(relName,RELCAT_RELNAME)==0||strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }
    int relId=OpenRelTable::getRelId(relName);
    if(relId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    return OpenRelTable::closeRel(relId);
}


// renameRel
int Schema::renameRel(char oldRelName[ATTR_SIZE],char newRelName[ATTR_SIZE])
{
    // Catalog relations cannot be renamed
    if (strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
        strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 ||
        strcmp(newRelName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Relation must be closed before modifying its schema
    int relId = OpenRelTable::getRelId(oldRelName);

    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Perform actual rename in Block Access layer
    return BlockAccess::renameRelation(
        oldRelName,
        newRelName
    );
}

// renameAttr()
int Schema::renameAttr(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]
) {
    // Catalog relations cannot have their attributes renamed
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Relation must be closed before modifying its schema
    int relId = OpenRelTable::getRelId(relName);

    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Perform actual rename in Block Access layer
    return BlockAccess::renameAttribute(
        relName,
        oldAttrName,
        newAttrName
    );
}


// createRel()
// Creates a new relation and adds its entries to RELATIONCAT and ATTRIBUTECAT.
int Schema::createRel(char relName[], int numOfAttributes,
    char attrNames[][ATTR_SIZE], int attrType[]) {

    // Convert relation name into an Attribute for searching.
    Attribute relNameAsAttribute;
    strcpy(relNameAsAttribute.sVal, relName);

    // Start searching RELATIONCAT from the beginning.
    int ret = RelCacheTable::resetSearchIndex(RELCAT_RELID);

    if (ret != SUCCESS) {
        return ret;
    }

    // Search RELATIONCAT for the given relation name.
    char relCatAttrName[ATTR_SIZE];
    strcpy(relCatAttrName, RELCAT_ATTR_RELNAME);

    RecId targetRelId = BlockAccess::linearSearch(
        RELCAT_RELID,         // relId
        relCatAttrName,       // attrName
        relNameAsAttribute,   // attrVal
        EQ                    // op
    );

    // Relation already exists.
    if (targetRelId.block != INVALID_BLOCKNUM) {
        return E_RELEXIST;
    }

    // Check whether any two attributes have the same name (in the relation we re about to create)
    for (int i = 0; i < numOfAttributes; i++) {
        for (int j = i + 1; j < numOfAttributes; j++) {

            if (strcmp(attrNames[i], attrNames[j]) == 0) {
                return E_DUPLICATEATTR;
            }
        }
    }

    // Build the RELATIONCAT record.
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    strcpy( relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName );
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = numOfAttributes;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = INVALID_BLOCKNUM;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = INVALID_BLOCKNUM;
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal =
        floor( 2016.0 / (ATTR_SIZE * numOfAttributes + 1));      // Calculate number of records that fit in one record block.


    // Insert relation entry into RELATIONCAT
    ret = BlockAccess::insert( RELCAT_RELID, relCatRecord );
    if (ret != SUCCESS) {
        return ret;
    }

    // Insert one ATTRIBUTECAT record for every attribute.
    for (int i = 0; i < numOfAttributes; i++) {

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        // Relation name.
        strcpy( attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName );

        // Attribute name.
        strcpy( attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrNames[i] );

        // Attribute type.
        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrType[i];

        // No primary key at this stage.
        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = INVALID_BLOCKNUM;

        // No B+ tree index yet.
        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = INVALID_BLOCKNUM;

        // Attribute position inside the record.
        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;


        // Insert attribute entry into ATTRIBUTECAT.
        ret = BlockAccess::insert( ATTRCAT_RELID, attrCatRecord );
        if (ret != SUCCESS) {
            Schema::deleteRel(relName);    // Remove the partially-created relation.
            return E_DISKFULL;              // documentation specifies E_DISKFULL here.
        }
    }

    return SUCCESS;
}

//deleteRel()
// Deletes a relation after verifying that it is closed.
int Schema::deleteRel(char relName[ATTR_SIZE]) {

    // Catalog relations cannot be deleted.
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // check whether the relation is currently open.
    int relId = OpenRelTable::getRelId(relName);
    if (relId != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // delete the relation through the Block Access layer.
    return BlockAccess::deleteRelation(relName);
}


// STAGE 11 - createIndex()
int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE]) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return BPlusTree::bPlusCreate(relId, attrName);
}

// dropIndex()
int Schema::dropIndex(char *relName, char *attrName) {
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  if (AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry) != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  int rootBlock = attrCatEntry.rootBlock;
  if (rootBlock == -1) {
    return E_NOINDEX;
  }

  BPlusTree::bPlusDestroy(rootBlock);

  attrCatEntry.rootBlock = -1;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

  return SUCCESS;
}
