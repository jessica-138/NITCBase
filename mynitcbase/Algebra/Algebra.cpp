#include "Algebra.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>
using namespace std;

bool isNumber(char *str);


int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) 
{
    // Check whether the source relation is open.
    int srcRelId = OpenRelTable::getRelId(srcRel);    
    if (srcRelId == E_RELNOTOPEN)  {
        return E_RELNOTOPEN;
    }

    // Get the attribute information and verify that the attribute exists.
    AttrCatEntry attrCatEntry; 
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if (ret != SUCCESS) 
        {
        return E_ATTRNOTEXIST;
    }

    // Convert the condition value to the attribute's data type.
    /*** Convert strVal to an attribute of data type NUMBER or STRING ***/
    int type = attrCatEntry.attrType;
    Attribute attrVal;
    if (type == NUMBER) {
        if (isNumber(strVal))  {       
            attrVal.nVal = atof(strVal);   // converts string to num
        } 
        else {
            return E_ATTRTYPEMISMATCH;   // checks type mismatch
        }
    } 
    else if (type == STRING){
        strcpy(attrVal.sVal, strVal);
    }

    // Get source relation metadata and build the target schema.
    /*** Creating and opening the target relation ***/
    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
    if (ret != SUCCESS)
    {
        return ret;
    }

    int src_nAttrs = relCatEntry.numAttrs;
    char attr_names[src_nAttrs][ATTR_SIZE];
    int attr_types[src_nAttrs];
    for (int i = 0; i < src_nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        if (ret != SUCCESS)
        {
        return ret;
        }
        strcpy(attr_names[i], attrCatEntry.attrName);
        attr_types[i] = attrCatEntry.attrType;
    }

    // Create and open the target relation.
    ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
    if (ret != SUCCESS) {
        return ret;
    }

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Selecting and inserting records into the target relation ***/
    // Search matching records and insert them into the target.
    Attribute record[src_nAttrs];   // create space for 1 record

    RelCacheTable::resetSearchIndex(srcRelId);
    // search thru the source relid for records that match the condition

    // int count = 0;
    while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS)
    {
        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }

        // count = count + 1;
    }

    // Close the target relation after selection is complete.
    Schema::closeRel(targetRel);
    // std::cout << "number of records:" << count;
    
    return SUCCESS;
}



bool isNumber(char *str) {
  int len;
  float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
  */
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}



int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE])
{
  if (strcmp(relName, "RELATIONCAT") == 0 || strcmp(relName, "ATTRIBUTECAT") == 0){
    return E_NOTPERMITTED;
  }

  // get the relation's rel-id using OpenRelTable::getRelId() method
  int relId = OpenRelTable::getRelId(relName);

  // if relation is not open in open relation table, return E_RELNOTOPEN
  if (relId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  // get the relation catalog entry from relation cache
  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  /* if relCatEntry.numAttrs != numberOfAttributes in relation, return E_NATTRMISMATCH */
  if (relCatEntry.numAttrs != nAttrs)
  {
    return E_NATTRMISMATCH;
  }

  Attribute recordValues[nAttrs] = {};

  //Converting 2D char array of record values to Attribute array recordValues
  for (int i = 0; i < nAttrs; i++)
  {
    // get the attr-cat entry for the i'th attribute from the attr-cache
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    if (ret != SUCCESS)
    {
      return ret;
    }

    int type = attrCatEntry.attrType;
    if (type == NUMBER)
    {
      if (isNumber(record[i]))
      {
        recordValues[i].nVal = atof(record[i]);
      }
      else
      {
        return E_ATTRTYPEMISMATCH;
      }
    }
    else if (type == STRING)
    {
      strcpy(recordValues[i].sVal, record[i]);
    }
  }

  int retVal = BlockAccess::insert(relId, recordValues);
  return retVal;
}


int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {

    // Get the relID of the source relation.
    int srcRelId = OpenRelTable::getRelId(srcRel);

    // The source relation must be open before projection.
    if (srcRelId == E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }

    // Get metadata of the source relation, such as number of attributes.
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    // Stop if the relation metadata could not be obtained.
    if (ret != SUCCESS){
        return ret;
    }

    // Get the number of attributes in the source relation.
    int numAttrs = relCatEntry.numAttrs;

    // Arrays to store the attribute names and their data types (required for putting in new relation)
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    // Read the metadata of every attribute from the attribute cache.
    for (int i = 0; i < numAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        
        // Stop if attribute metadata could not be obtained.
        if (ret != SUCCESS){
            return ret;
        }

        // Store the attribute name and type for creating the target relation.
        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    /*** Creating and opening the target relation ***/

    // Create the target relation with the same attributes as the source.
    ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
    if (ret != SUCCESS){      // If creation fails, return the error.
        return ret;
    }

    // Open the newly created target relation and get its rel ID.
    int targetRelId = OpenRelTable::openRel(targetRel);


    if (targetRelId < 0) {                // If opening fails, delete the target relation and return error
        Schema::deleteRel(targetRel);
        return targetRelId;
    }


    /*** Inserting projected records into the target relation ***/

    // Start reading records from the beginning of the source relation.
    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[numAttrs];              // Temporary array to hold one record at a time.


    //  int count = 0;
    // Get each record from the source relation one by one.
    while (BlockAccess::project(srcRelId, record) == SUCCESS){   
        // Insert the current record into the target relation.
        ret = BlockAccess::insert(targetRelId, record);
        
        // If insertion fails, close and delete the partially created target.
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }

        // count++;
    }

    Schema::closeRel(targetRel);         // Close the target relation after all records are copied.

    // cout << "number of records :" << count;

    // Projection completed successfully.
    return SUCCESS;
    }


// project (but only selected attributes)
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {
    
    // Get rel id of src relation
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    // Get metadata of source relation
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
    if (ret != SUCCESS)
    {
        return ret;
    }

    // Get number of source attributes and prepare metadata arrays.
    int src_nAttrs = relCatEntry.numAttrs;
    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    /*** Checking if attributes of target are present in the source relation ***/
    
    // Check that every requested attribute exists in the source.
    char attr_names[tar_nAttrs][ATTR_SIZE];
    for (int i = 0; i < tar_nAttrs; i++)  {
            AttrCatEntry attrCatEntry;
            ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
            if (ret != SUCCESS){
                return E_ATTRNOTEXIST;
            }

        // Store the position, type and name of each requested attribute.
        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
        strcpy(attr_names[i], attrCatEntry.attrName);
    }


    /*** Creating and opening the target relation ***/
    
    // Create the target with only the selected attributes.
    ret = Schema::createRel(targetRel, tar_nAttrs, attr_names, attr_types);
    if (ret != SUCCESS){
        return ret;
    } 

    // Open  newly created target relation.
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0){
        // Delete target if opening it fails.
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/
    
    // Start reading source records from the beginning
    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[src_nAttrs];

    // int count = 0;
    // Read each complete source record one by one
    while (BlockAccess::project(srcRelId, record) == SUCCESS) {

        // Create a record containing only the requested attributes
        Attribute proj_record[tar_nAttrs];

        // Copy selected attributes using their offsets
        for (int attr_iter = 0; attr_iter < tar_nAttrs; attr_iter++){
            proj_record[attr_iter] = record[attr_offset[attr_iter]];
        }

        // Insert the projected record into the target
        
        ret = BlockAccess::insert(targetRelId, proj_record);

        if (ret != SUCCESS)  {
            // Clean up the target if insertion fails.
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }

        // count++;
    }

    // Close the target after all records are inserted.
    Schema::closeRel(targetRel);
    
    // cout << "number of records: " << count;

    // Projection completed successfully.
    return SUCCESS;
}