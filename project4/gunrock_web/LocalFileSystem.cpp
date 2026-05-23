#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <cstring>
#include <algorithm>

#include "LocalFileSystem.h"
#include "ufs.h"
#include "Disk.h" 

using namespace std;

LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) { //read file system metadata
  char buffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  //use function to calculate how many blocks the inode bitmap takes up
  //read inode allocation information
  int numBitBlocks = (super->num_inodes / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  //num_inodes from ufs.h

  for (int i = 0; i < numBitBlocks; i++) {
    disk->readBlock(super->inode_bitmap_addr + i, inodeBitmap + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
  //same function as readInodeBitmap but we use writeblock instead
  //write inode allocation information
  int numBitBlocks = (super->num_inodes / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  for (int i = 0; i < numBitBlocks; i++) {
    disk->writeBlock(super->inode_bitmap_addr + i, inodeBitmap + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  //same function but we use data_bitmap_addr and dataBitmap
  //read data block allocation information
  int numBitBlocks = (super->num_data / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  for (int i = 0; i < numBitBlocks; i++) {
    disk->readBlock(super->data_bitmap_addr + i, dataBitmap + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  //same function as readDataBitmap but we use writeblock instead
  //write data block allocation information
  int numBitBlocks = (super->num_data / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  for (int i = 0; i < numBitBlocks; i++) {
    disk->writeBlock(super->data_bitmap_addr + i, dataBitmap + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  //calculate how many blocks the inode region occupies
  //inode region starts at super->inode_region_addr with super->num_inodes entries
  //READ ALL INODES
  int numInodeBlocks = (super->num_inodes * sizeof(inode_t) + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  for (int i = 0; i < numInodeBlocks; i++) {
    disk->readBlock(super->inode_region_addr + i, (void *)((char *)inodes + (i * UFS_BLOCK_SIZE)));
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  //WRITE ALL INODES BACK TO INODE REGION
  int numInodeBlocks = (super->num_inodes * sizeof(inode_t) + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  for (int i = 0; i < numInodeBlocks; i++) {
    disk->writeBlock(super->inode_region_addr + i, (void *)((char *)inodes + (i * UFS_BLOCK_SIZE)));
  }
}

//MAIN IMPLEMENTATION OF FUNCTIONS

int LocalFileSystem::lookup(int parentInodeNumber, string name) { //lookup will find files in directories
  super_t super;
  readSuperBlock(&super);
  
  //validate parent inode number
  if (parentInodeNumber < 0 || parentInodeNumber >= super.num_inodes) {
    return -1;
  }
  
  //read all inodes
  inode_t *inodes = new inode_t[super.num_inodes];
  readInodeRegion(&super, inodes);
  
  inode_t parentInode = inodes[parentInodeNumber];
  
  //directory entries from parent inode's blocks
  dir_ent_t dirEntry;
  int totalBytesRead = 0;
  
  for (int blockIdx = 0; blockIdx < DIRECT_PTRS && totalBytesRead < parentInode.size; blockIdx++) {
    int dataBlockNum = parentInode.direct[blockIdx];
    if (dataBlockNum == 0 || dataBlockNum < 0) break;  // No more blocks or invalid
    
    char blockBuff[UFS_BLOCK_SIZE]; //buffer for blocks
    disk->readBlock(dataBlockNum, blockBuff);
    
    //calculate how many entries to read from this block
    int bytesLeftInDir = parentInode.size - totalBytesRead;
    int numEntriesInBlock = bytesLeftInDir / (int)sizeof(dir_ent_t);
    if (numEntriesInBlock > (int)(UFS_BLOCK_SIZE / sizeof(dir_ent_t))) {
      numEntriesInBlock = UFS_BLOCK_SIZE / sizeof(dir_ent_t);
    }
    
    for (int i = 0; i < numEntriesInBlock; i++) {
      memcpy(&dirEntry, blockBuff + (i * sizeof(dir_ent_t)), sizeof(dir_ent_t));
      
      if (dirEntry.inum == -1) continue;  //unused entry
      
      if (string(dirEntry.name) == name) {
        delete[] inodes;
        return dirEntry.inum;
      }
    }
    
    totalBytesRead += numEntriesInBlock * (int)sizeof(dir_ent_t);
  }
  
  delete[] inodes;
  return -1;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super;
  readSuperBlock(&super);

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes) {
    return -1; //invalid inode num
  }

  inode_t *inodes = new inode_t[super.num_inodes];
  readInodeRegion(&super, inodes);

  *inode = inodes[inodeNumber];
  delete[] inodes;

  return 0; //returning 0 means it was successful
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  super_t super;
  readSuperBlock(&super);
  
  inode_t *inodes = new inode_t[super.num_inodes];
  readInodeRegion(&super, inodes);
  
  inode_t fileInode = inodes[inodeNumber];
  delete[] inodes;
  
  //handle size: if size is larger than file size, only return file size
  int bytesToRead = min(size, (int)fileInode.size);
  int bytesRead = 0;
  
  //read from direct pointers
  for (int blockIdx = 0; blockIdx < DIRECT_PTRS && bytesRead < bytesToRead; blockIdx++) {
    int dataBlockNum = fileInode.direct[blockIdx];
    if (dataBlockNum == 0) break;
    
    char blockBuffer[UFS_BLOCK_SIZE];
    disk->readBlock(dataBlockNum, blockBuffer);
    
    int bytesToCopy = min(UFS_BLOCK_SIZE, bytesToRead - bytesRead); //bytes to copy from this block
    memcpy((char *)buffer + bytesRead, blockBuffer, bytesToCopy);
    bytesRead += bytesToCopy;
  }
  
  return bytesRead;
}

//implement later

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  disk->beginTransaction(); //seen in the README
  super_t super;
  readSuperBlock(&super);

  //read all structures
  unsigned char *inodeBitmap = new unsigned char[(super.num_inodes + 7) / 8];
  unsigned char *dataBitmap = new unsigned char[(super.num_data + 7) / 8];
  inode_t *inodes = new inode_t[super.num_inodes];

  readInodeBitmap(&super, inodeBitmap);
  readDataBitmap(&super, dataBitmap);
  readInodeRegion(&super, inodes);

  //stage: check if entry already exists
  int existingInode = lookup(parentInodeNumber, name);
  if (existingInode >= 0) {
    if (inodes[existingInode].type == type) {
      //entry already exists with correct type, return success
      delete[] inodeBitmap;
      delete[] dataBitmap;
      delete[] inodes;
      disk->commit(); //seen in the README
      return existingInode;
    } else {
      //entry already exists with wrong type, return error
      delete[] inodeBitmap;
      delete[] dataBitmap;
      delete[] inodes;
      disk->rollback(); //seen in the README
      return -1;
    }
  }

  //stage: find and allocate lowest free inode
  int freeInode = -1;
  for (int i = 0; i < super.num_inodes; i++) {
    int byteIdx = i / 8;
    int bitIdx = i % 8;
    if (!(inodeBitmap[byteIdx] & (1 << bitIdx))) {
      freeInode = i;
      break;
    }
  }

  if (freeInode < 0) {
    //no free inodes
    delete[] inodeBitmap;
    delete[] dataBitmap;
    delete[] inodes;
    disk->rollback();
    return -1;
  }
  //stage: mark inode as allocated
  inodeBitmap[freeInode / 8] |= (1 << (freeInode % 8));

  //initialize inode
  inodes[freeInode].type = type;
  inodes[freeInode].size = 0;
  for (int i = 0; i < DIRECT_PTRS; i++) {
    inodes[freeInode].direct[i] = 0;
  }

  //stage: if directory, allocate block for . and ..
  if (type == UFS_DIRECTORY) {
    int freeDataBlock = -1;
    for (int i = 0; i < super.num_data; i++) {
      int byteIdx = i / 8;
      int bitIdx = i % 8;
      if (!(dataBitmap[byteIdx] & (1 << bitIdx))) {
        freeDataBlock = i;
        break;
      }
    }
    if (freeDataBlock < 0) {
      // No space - free allocated inode
      inodeBitmap[freeInode / 8] &= ~(1 << (freeInode % 8));
      delete[] inodeBitmap;
      delete[] dataBitmap;
      delete[] inodes;
      disk->rollback();
      return -1;
    }
    //mark data block as allocated
    dataBitmap[freeDataBlock / 8] |= (1 << (freeDataBlock % 8));
    
    //create . and ..
    char blockBuf[UFS_BLOCK_SIZE];
    memset(blockBuf, 0, UFS_BLOCK_SIZE);
    dir_ent_t *entries = (dir_ent_t *)blockBuf;
    memset(entries[0].name, 0, 28);
    entries[0].name[0] = '.'; //. (current directory)
    entries[0].inum = freeInode;

    memset(entries[1].name, 0, 28);
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].inum = parentInodeNumber; //.. (parent directory)

    disk->writeBlock(super.data_region_addr + freeDataBlock, blockBuf);
    inodes[freeInode].direct[0] = super.data_region_addr + freeDataBlock;
    inodes[freeInode].size = 2 * sizeof(dir_ent_t);
  }

  //stage: add entry to parent directory
  inode_t &parentInode = inodes[parentInodeNumber];
  //find space in parent's blocks or allocate new one
  int blockNum = -1;
  int offsetBlock = -1;
  for (int i = 0; i < DIRECT_PTRS && blockNum < 0; i++) {
    if (parentInode.direct[i] == 0) {
      break;
    }
    char blockBuf[UFS_BLOCK_SIZE];
    disk->readBlock(parentInode.direct[i], blockBuf);
    for (int j = 0; j < (int)(UFS_BLOCK_SIZE / sizeof(dir_ent_t)); j++) {
      dir_ent_t *entry = (dir_ent_t *)(blockBuf + j * sizeof(dir_ent_t));
      if (entry->inum == -1) {
        blockNum = parentInode.direct[i];
        offsetBlock = j;
        break;
      }
    }
  }
  //if no space, allocate new block
  if (blockNum < 0) {
    int freeDataBlock = -1;
    for (int i = 0; i < super.num_data; i++) {
      int byteIdx = i / 8;
      int bitIdx = i % 8;
      if (!(dataBitmap[byteIdx] & (1 << bitIdx))) {
        freeDataBlock = i;
        break;
      }
    }
    if (freeDataBlock < 0) {
      //cleanup and error, no free space
      inodeBitmap[freeInode / 8] &= ~(1 << (freeInode % 8)); //free allocated inode
      delete[] inodeBitmap;
      delete[] dataBitmap;
      delete[] inodes;
      disk->rollback();
      return -1;
    }
    //mark data block as allocated
    dataBitmap[freeDataBlock / 8] |= (1 << (freeDataBlock % 8));
    blockNum = super.data_region_addr + freeDataBlock;
    offsetBlock = 0;
    //add to parent's direct pointers
    for (int i = 0; i < DIRECT_PTRS; i++) {
      if (parentInode.direct[i] == 0) {
        parentInode.direct[i] = blockNum;
        break;
      }
    }
  }

  //stage: write new entry to parent block
  char blockBuf[UFS_BLOCK_SIZE];
  disk->readBlock(blockNum, blockBuf);
  dir_ent_t newEntry;
  memset(newEntry.name, 0, 28);
  strncpy(newEntry.name, name.c_str(), 28);
  newEntry.inum = freeInode;

  memcpy(blockBuf + offsetBlock * sizeof(dir_ent_t), &newEntry, sizeof(dir_ent_t));
  disk->writeBlock(blockNum, blockBuf);

  //update parent inode size if needed
  parentInode.size += sizeof(dir_ent_t);

  //final stage: WRITE EVERYTHING BACK to disk after processing
  writeInodeBitmap(&super, inodeBitmap);
  writeDataBitmap(&super, dataBitmap);
  writeInodeRegion(&super, inodes);
  disk->writeBlock(0, &super); //write superblock back in case we need to update any info there

  //cleanup
  delete[] inodeBitmap;
  delete[] dataBitmap;
  delete[] inodes;

  disk->commit();
  return freeInode;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  disk->beginTransaction();
  super_t super;
  readSuperBlock(&super);

  //read all structures
  unsigned char *dataBitmap = new unsigned char[(super.num_data + 7) / 8];
  inode_t *inodes = new inode_t[super.num_inodes];
  readDataBitmap(&super, dataBitmap);
  readInodeRegion(&super, inodes);

  inode_t &fileInode = inodes[inodeNumber];
  //CALCULATE how many blocks needed for this file
  int blocksNeeded = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  int blocksHad = (fileInode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  int bytesWritten = 0;
  //reuse existing blocks if possible as first priority
  for (int i = 0; i < blocksNeeded && i < DIRECT_PTRS; i++) {
    int blockNum = fileInode.direct[i];
    
    //allocate if needed
    if (blockNum == 0) {
      int newBlock = -1;
      for (int j = 0; j < super.num_data; j++) {
        int byteIdx = j / 8;
        int bitIdx = j % 8;
        if (!(dataBitmap[byteIdx] & (1 << bitIdx))) {
          newBlock = j;
          break;
        }
      }
      
      if (newBlock < 0) {
        //if out of space, we write until we dont have enough space and return that
        fileInode.size = bytesWritten;
        writeDataBitmap(&super, dataBitmap);
        writeInodeRegion(&super, inodes);
        disk->writeBlock(0, (void *)&super);
        
        delete[] dataBitmap;
        delete[] inodes;
        disk->commit();
        return bytesWritten;
      }
      
      dataBitmap[newBlock / 8] |= (1 << (newBlock % 8));
      blockNum = super.data_region_addr + newBlock;
      fileInode.direct[i] = blockNum;
    }
    
    // Write data to block
    int bytesToWrite = min(UFS_BLOCK_SIZE, size - bytesWritten);
    disk->writeBlock(blockNum, (char *)buffer + bytesWritten);
    bytesWritten += bytesToWrite;
  }

  //if file shrunk, free the extra blocks not used anymore
  for (int i = blocksNeeded; i < blocksHad && i < DIRECT_PTRS; i++) {
    int blockNum = fileInode.direct[i];
    if (blockNum != 0) {
      int dataBlockIdx = blockNum - super.data_region_addr;
      dataBitmap[dataBlockIdx / 8] &= ~(1 << (dataBlockIdx % 8)); //mark block as free
      fileInode.direct[i] = 0;
    }
  }

  //update inode size and write everything back to disk
  fileInode.size = bytesWritten;
  writeDataBitmap(&super, dataBitmap);
  writeInodeRegion(&super, inodes);
  disk->writeBlock(0, (void *)&super);

  //cleanup
  delete[] dataBitmap;
  delete[] inodes;

  disk->commit();
  return bytesWritten;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  disk->beginTransaction();
  super_t super;
  readSuperBlock(&super);

  //read all structures
  unsigned char *inodeBitmap = new unsigned char[(super.num_inodes + 7) / 8];
  unsigned char *dataBitmap = new unsigned char[(super.num_data + 7) / 8];
  inode_t *inodes = new inode_t[super.num_inodes];
  readInodeBitmap(&super, inodeBitmap);
  readDataBitmap(&super, dataBitmap);
  readInodeRegion(&super, inodes);

  //stage: find entry in parent directory
  int targetInode = lookup(parentInodeNumber, name);
  if (targetInode < 0) {
    //entry doesn't exist, return error
    delete[] inodeBitmap;
    delete[] dataBitmap;
    delete[] inodes;
    disk->rollback();
    return -1;
  }

  //stage: reject unlinking . or ..
  if (name == "." || name == "..") {
    delete[] inodeBitmap;
    delete[] dataBitmap;
    delete[] inodes;
    disk->rollback();
    return -1;
  }

  inode_t &target = inodes[targetInode];
  //stage: if directory, check if empty (only . and ..)
  if (target.type == UFS_DIRECTORY) {
    if (target.size != 2 * sizeof(dir_ent_t)) {
      delete[] inodeBitmap;
      delete[] dataBitmap;
      delete[] inodes;
      disk->rollback();
      return -1; // Directory not empty
    }
  }

  //stage: free all data blocks used by file/directory
  for (int i = 0; i < DIRECT_PTRS; i++) {
    if (target.direct[i] != 0) {
      int dataBlockIdx = target.direct[i] - super.data_region_addr;
      dataBitmap[dataBlockIdx / 8] &= ~(1 << (dataBlockIdx % 8)); //mark block as free
    } else {
      break;
    }
  }

  //stage: free inode
  inodeBitmap[targetInode / 8] &= ~(1 << (targetInode % 8));
  //stage: remove entry from parent directory
  inode_t &parentInode = inodes[parentInodeNumber];

  //find and remove entry from parent directory blocks
  for (int i = 0; i < DIRECT_PTRS && i * UFS_BLOCK_SIZE < parentInode.size; i++) {
    int blockNum = parentInode.direct[i];
    if (blockNum == 0) break;
    
    char blockBuf[UFS_BLOCK_SIZE];
    disk->readBlock(blockNum, blockBuf);
    
    int entriesInBlock = UFS_BLOCK_SIZE / sizeof(dir_ent_t);
    for (int j = 0; j < entriesInBlock; j++) {
      dir_ent_t *ent = (dir_ent_t *)(blockBuf + (j * sizeof(dir_ent_t)));
      if (ent->inum == targetInode) {
        ent->inum = -1; //mark as unused
        disk->writeBlock(blockNum, blockBuf);
  //stage: write all changes to disk
        writeInodeBitmap(&super, inodeBitmap);
        writeDataBitmap(&super, dataBitmap);
        writeInodeRegion(&super, inodes);
        disk->writeBlock(0, (void *)&super);
        
        delete[] inodeBitmap;
        delete[] dataBitmap;
        delete[] inodes;
        
        disk->commit();
        return 0;
      }
    }
  }

  //cleanup but error if it gets here
  delete[] inodeBitmap;
  delete[] dataBitmap;
  delete[] inodes;
  disk->commit();
  return -1;
}

