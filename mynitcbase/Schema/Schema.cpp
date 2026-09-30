#include "Schema.h"

#include <cmath>
#include <cstring>

// openRel
int Schema::openRel(char relName[ATTR_SIZE]) {
    return OpenRelTable::openRel(relName);
}

// closeRel
int Schema::closeRel(char relName[ATTR_SIZE]) {

    int relId = OpenRelTable::getRelId(relName);

    if (relId == E_RELNOTOPEN) {
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

