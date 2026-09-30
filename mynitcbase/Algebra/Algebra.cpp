#include "Algebra.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

bool isNumber(char *str) {
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);

    return ret == 1 && len == strlen(str);
}

int Algebra::select(
    char srcRel[ATTR_SIZE],
    char targetRel[ATTR_SIZE],
    char attr[ATTR_SIZE],
    int op,
    char strVal[ATTR_SIZE]
) {

    // Get rel-id of source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }


    // Get information about the attribute being searched
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        srcRelId,
        attr,
        &attrCatEntry
    );

    if (ret != SUCCESS) {
        return E_ATTRNOTEXIST;
    }


    // Convert strVal into an Attribute
    Attribute attrVal;

    if (attrCatEntry.attrType == NUMBER) {

        if (isNumber(strVal)) {
            attrVal.nVal = atof(strVal);
        }
        else {
            return E_ATTRTYPEMISMATCH;
        }

    }
    else if (attrCatEntry.attrType == STRING) {

        strcpy(attrVal.sVal, strVal);
    }


    // Start searching from the beginning
    ret = RelCacheTable::resetSearchIndex(srcRelId);

    if (ret != SUCCESS) {
        return ret;
    }


    // Get information about the source relation
    RelCatEntry relCatEntry;

    ret = RelCacheTable::getRelCatEntry(
        srcRelId,
        &relCatEntry
    );

    if (ret != SUCCESS) {
        return ret;
    }


    // Print column headings
    printf("|");

    for (int i = 0; i < relCatEntry.numAttrs; i++) {

        AttrCatEntry currentAttr;

        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId,
            i,
            &currentAttr
        );

        if (ret != SUCCESS) {
            return ret;
        }

        printf(" %s |", currentAttr.attrName);
    }

    printf("\n");


    // Keep searching for matching records
    while (true) {

        RecId searchRes = BlockAccess::linearSearch(
            srcRelId,
            attr,
            attrVal,
            op
        );


        // No more matching records
        if (searchRes.block == -1 &&
            searchRes.slot == -1) {

            break;
        }


        // Read the matching record
        RecBuffer recBuffer(searchRes.block);

        Attribute record[relCatEntry.numAttrs];

        ret = recBuffer.getRecord(
            record,
            searchRes.slot
        );

        if (ret != SUCCESS) {
            return ret;
        }


        // Print the record
        printf("|");

        for (int i = 0; i < relCatEntry.numAttrs; i++) {

            AttrCatEntry currentAttr;

            ret = AttrCacheTable::getAttrCatEntry(
                srcRelId,
                i,
                &currentAttr
            );

            if (ret != SUCCESS) {
                return ret;
            }

            if (currentAttr.attrType == NUMBER) {
                printf(" %g |", record[i].nVal);
            }
            else {
                printf(" %s |", record[i].sVal);
            }
        }

        printf("\n");
    }

    return SUCCESS;
}



// stage 7 imp 
// INSERT
int Algebra::insert(char relName[ATTR_SIZE], int numberOfAttributes, char record[][ATTR_SIZE]) {
    /*
     * Insert a record into an existing relation.
     *
     * 1. Do not allow direct insertion into the catalogs.
     * 2. Check that the relation is open.
     * 3. Check that the number of attributes matches.
     * 4. Convert each string value into an Attribute.
     * 5. Pass the converted record to BlockAccess::insert().
     */

    // Catalogs cannot be directly modified by INSERT
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Get the relation ID
    int relId = OpenRelTable::getRelId(relName);

    // Relation must be open
    if (relId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get relation information from the cache
    RelCatEntry relCatEntry;

    int ret = RelCacheTable::getRelCatEntry(
        relId,
        &relCatEntry
    );

    if (ret != SUCCESS) {
        return ret;
    }

    // Number of values must match number of attributes
    if (relCatEntry.numAttrs != numberOfAttributes) {
        return E_NATTRMISMATCH;
    }

    // This will hold the converted Attribute values
    Attribute recordValues[numberOfAttributes];

    // Convert each input value
    for (int i = 0; i < numberOfAttributes; i++) {

        AttrCatEntry attrCatEntry;

        // Get information about this attribute
        ret = AttrCacheTable::getAttrCatEntry(
            relId,
            i,
            &attrCatEntry
        );

        if (ret != SUCCESS) {
            return ret;
        }

        // If attribute is NUMBER
        if (attrCatEntry.attrType == NUMBER) {

            // Input must be a valid number
            if (!isNumber(record[i])) {
                return E_ATTRTYPEMISMATCH;
            }

            // Convert string → double
            recordValues[i].nVal = atof(record[i]);
        }

        // If attribute is STRING
        else if (attrCatEntry.attrType == STRING) {

            // Copy string into Attribute
            strcpy(recordValues[i].sVal, record[i]);
        }
    }

    // Send converted record to Block Access layer
    return BlockAccess::insert(relId, recordValues);
}