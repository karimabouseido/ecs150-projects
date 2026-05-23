#include <iostream>
#include <string>
#include <vector>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

//make directory command in local file system on disk
//takes parent inode number and directory name, creates directory in parent directory with given name
//USE: ./ds3mkdir disk_image.img parent_inode directory

__attribute__((no_sanitize_address))
int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile parentInode directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " a.img 0 a" << endl;
    return 1;
  }

  // Parse command line arguments

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int parentInode = stoi(argv[2]);
  string directory = string(argv[3]);

  //create directory
  int result = fileSystem->create(parentInode, UFS_DIRECTORY, directory);
  //error handling
  if (result < 0) {
    cerr << "Error creating directory" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  //cleanup
  delete fileSystem;
  delete disk;
  return 0;
}
