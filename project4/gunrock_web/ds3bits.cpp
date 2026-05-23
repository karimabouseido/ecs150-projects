#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

//print the file system metadata: superblock info, inode bitmap, and data bitmap for a given disk image

//USE: ./ds3bits disk_image.img

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  
  //read superblock
  super_t super;
  fileSystem->readSuperBlock(&super);
  
  //print superblock info
  cout << "Super" << endl;
  cout << "inode_region_addr " << super.inode_region_addr << endl;
  cout << "inode_region_len " << super.inode_region_len << endl;
  cout << "num_inodes " << super.num_inodes << endl;
  cout << "data_region_addr " << super.data_region_addr << endl;
  cout << "data_region_len " << super.data_region_len << endl;
  cout << "num_data " << super.num_data << endl;
  cout << endl;
  
  //print inode bitmap
  cout << "Inode bitmap" << endl;
  int iSize = (super.num_inodes / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE * UFS_BLOCK_SIZE; //inode bitmap size in bytes
  unsigned char *iBitmap = new unsigned char[iSize];
  fileSystem->readInodeBitmap(&super, iBitmap);
  
  for (int i = 0; i < (super.num_inodes + 7) / 8; i++) {
    cout << (unsigned int)iBitmap[i] << " ";
  }
  cout << endl;
  cout << endl;
  
  //print data bitmap
  cout << "Data bitmap" << endl;
  int dSize = (super.num_data / 8 + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE * UFS_BLOCK_SIZE; //data bitmap size in bytes
  unsigned char *dBitmap = new unsigned char[dSize];
  fileSystem->readDataBitmap(&super, dBitmap);
  
  for (int i = 0; i < (super.num_data + 7) / 8; i++) {
    cout << (unsigned int)dBitmap[i] << " ";
  }
  cout << endl;
  
  delete[] iBitmap;
  delete[] dBitmap;
  delete fileSystem;
  delete disk;
  return 0;
}
