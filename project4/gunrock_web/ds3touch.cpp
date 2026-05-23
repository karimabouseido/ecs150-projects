#include <iostream>
#include <string>
#include <vector>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

__attribute__((no_sanitize_address))
int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile parentInode fileName" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " a.img 0 a.txt" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int parentInode = stoi(argv[2]);
  string fileName = string(argv[3]);

  //create empty file
  int result = fileSystem->create(parentInode, UFS_REGULAR_FILE, fileName);
  if (result < 0) {
    cerr << "Error creating file" << endl;
    return 1;
  }

  delete fileSystem;
  delete disk;
  return 0;
}
