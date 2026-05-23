#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

//prints file contents (like the cat command), takes an inode number, reads file, prints it
//USE: ./ds3cat disk_image.img inode_number

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int ino = stoi(argv[2]);
  
  //check if inode number is valid
  super_t super;
  char buf[UFS_BLOCK_SIZE];
  disk->readBlock(0, buf);
  memcpy(&super, buf, sizeof(super_t));
  
  if (ino < 0 || ino >= super.num_inodes) {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  
  //verify inode is allocated
  int bitmapBlocks = (super.num_inodes / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  unsigned char *ibmap = new unsigned char[bitmapBlocks * UFS_BLOCK_SIZE];
  fileSystem->readInodeBitmap(&super, ibmap);
  
  int byteIdx = ino / 8;
  int bitIdx = ino % 8;
  if (!(ibmap[byteIdx] & (1 << bitIdx))) {
    //inode not allocated
    cerr << "Error reading file" << endl;
    delete[] ibmap;
    delete fileSystem;
    delete disk;
    return 1;
  }
  delete[] ibmap;
  
  //get inode info
  inode_t inode;
  int result = fileSystem->stat(ino, &inode);
  if (result < 0) {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  
  //reject directories
  if (inode.type == UFS_DIRECTORY) {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  
  //print file blocks
  cout << "File blocks" << endl;
  //calculate blocks needed for this file
  int numBlocks = inode.size / UFS_BLOCK_SIZE;
  if ((inode.size % UFS_BLOCK_SIZE) != 0) {
    numBlocks += 1;
  }
  
  for (int i = 0; i < numBlocks; i++) {
    if (inode.direct[i] == 0) break;
    cout << inode.direct[i] << endl;
  }
  cout << endl;
  
  //print file data
  cout << "File data" << endl;
  
  char *data = new char[inode.size + 1];
  int nread = fileSystem->read(ino, data, inode.size);
  data[nread] = '\0';
  cout << data;
  
  delete[] data;
  delete fileSystem;
  delete disk;
  return 0;
}
