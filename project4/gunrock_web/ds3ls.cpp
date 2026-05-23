#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

//similar to ls command but for the local file system on disk
//prints inode number for each entry in the directory specified by the path, sorted by name. 
//if path is file, print file
//if path doesn't exist, print error

//USE ./ds3ls disk_image.img /path/to/directory

bool compareByName(const pair<int, string>& a, const pair<int, string>& b) {
  return a.second < b.second;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << argv[0] << ": diskImageFile directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
    return 1;
  }

  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string path = string(argv[2]);
  
  //traverse path to find inode
  int ino = 0;
  string rest = path;
  
  //skip leading slash
  if (rest[0] == '/') {
    rest = rest.substr(1);
  }
  
  //walk path components
  while (!rest.empty()) {
    size_t pos = rest.find('/');
    string name;
    
    if (pos == string::npos) {
      name = rest;
      rest = "";
    } else {
      name = rest.substr(0, pos);
      rest = rest.substr(pos + 1);
    }
    
    int next = fileSystem->lookup(ino, name);
    if (next < 0) {
      cerr << "Directory not found" << endl;
      delete fileSystem;
      delete disk;
      return 1;
    }
    ino = next;
  }
  
  //get inode info
  inode_t inode;
  int result = fileSystem->stat(ino, &inode);
  if (result < 0) {
    cerr << "Directory not found" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  
  //if file, print and exit
  if (inode.type == UFS_REGULAR_FILE) {
    string fname;
    size_t slash = path.rfind('/');
    if (slash != string::npos) {
      fname = path.substr(slash + 1);
    } else {
      fname = path;
    }
    cout << ino << "\t" << fname << endl;
    delete fileSystem;
    delete disk;
    return 0;
  }
  
  //read directory entries
  vector<pair<int, string>> entries;
  int total = 0;
  
  for (int i = 0; i < DIRECT_PTRS && total < inode.size; i++) {
    int blk = inode.direct[i];
    if (blk == 0 || blk < 0) break;
    
    char buf[UFS_BLOCK_SIZE];
    disk->readBlock(blk, buf);
    
    //count entries in this block
    int rem = inode.size - total;
    int cnt = rem / (int)sizeof(dir_ent_t);
    if (cnt > (int)(UFS_BLOCK_SIZE / sizeof(dir_ent_t))) {
      cnt = UFS_BLOCK_SIZE / sizeof(dir_ent_t);
    }
    
    for (int j = 0; j < cnt; j++) {
      dir_ent_t *ent = (dir_ent_t *)(buf + (j * sizeof(dir_ent_t)));
      if (ent->inum != -1 && ent->name[0] != '\0') {
        entries.push_back({ent->inum, string(ent->name)});
      }
    }
    
    total += cnt * sizeof(dir_ent_t);
  }
  
  //sort by name
  sort(entries.begin(), entries.end(), compareByName); //use function at top of file
  
  //print results
  for (auto& ent : entries) {
    cout << ent.first << "\t" << ent.second << endl;
  }
  
  delete fileSystem;
  delete disk;
  return 0;
}
