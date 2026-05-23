#include <iostream>
#include <string>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

__attribute__((no_sanitize_address))
int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string srcFile = string(argv[2]);
  int dstInode = stoi(argv[3]);

  //read source file from host filesystem
  int fd = open(srcFile.c_str(), O_RDONLY);
  if (fd < 0) {
    cerr << "Error opening source file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  //get file size
  off_t fileSize = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  //read entire file into buffer (pad to at least UFS_BLOCK_SIZE)
  int bufferSize = (fileSize < UFS_BLOCK_SIZE) ? UFS_BLOCK_SIZE : ((fileSize + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE) * UFS_BLOCK_SIZE;
  unsigned char *fileBuffer = new unsigned char[bufferSize]();
  ssize_t bytesRead = read(fd, fileBuffer, fileSize);
  close(fd);

  if (bytesRead != fileSize) {
    cerr << "Error reading source file" << endl;
    delete[] fileBuffer;
    delete fileSystem;
    delete disk;
    return 1;
  }

  //extract filename from srcFile path
  size_t lastSlash = srcFile.rfind('/');
  string fileName = (lastSlash != string::npos) ? srcFile.substr(lastSlash + 1) : srcFile;

  //create file in disk
  int newInode = fileSystem->create(dstInode, UFS_REGULAR_FILE, fileName);
  if (newInode < 0) {
    cerr << "Error creating file" << endl;
    delete[] fileBuffer;
    delete fileSystem;
    delete disk;
    return 1;
  }

  //write file contents
  int bytesWritten = fileSystem->write(newInode, fileBuffer, fileSize);
  if (bytesWritten < 0) {
    cerr << "Error writing to file" << endl;
    delete[] fileBuffer;
    delete fileSystem;
    delete disk;
    return 1;
  }

  delete[] fileBuffer;
  delete fileSystem;
  delete disk;
  return 0;
}
