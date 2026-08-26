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