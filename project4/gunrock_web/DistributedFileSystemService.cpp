#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>
#include <vector>

#include "DistributedFileSystemService.h"
#include "ClientError.h"
#include "ufs.h"
#include "WwwFormEncodedDict.h"

using namespace std;

//given class function
DistributedFileSystemService::DistributedFileSystemService(string diskFile) : HttpService("/ds3/")
{
  this->fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}

//additional functions for path resolution and splitting
static int resolvePath(LocalFileSystem *fs, int startInode, const vector<string> &components)
{
  int cur = startInode;
  for (size_t i = 0; i < components.size(); ++i) {
    int next = fs->lookup(cur, components[i]);
    if (next < 0)
      throw ClientError::notFound();

    // Non-final components must be directories.
    if (i + 1 < components.size()) {
      inode_t st;
      fs->stat(next, &st);
      if (st.type != UFS_DIRECTORY)
        throw ClientError::notFound();
    }
    cur = next;
  }
  return cur;
}

static vector<string> splitPath(const string &path)
{
  vector<string> parts;
  stringstream ss(path);
  string token;
  while (getline(ss, token, '/'))
    if (!token.empty())
      parts.push_back(token);
  return parts;
}

//get function

void DistributedFileSystemService::get(HTTPRequest *request, HTTPResponse *response)
{
  string path = request->getPath();

  //remove the ds3 prefix
  string fsPath = path.substr(5);

  vector<string> parts = splitPath(fsPath);

  // Empty parts -> root directory.
  int curInode = UFS_ROOT_DIRECTORY_INODE_NUMBER;
  if (!parts.empty())
    curInode = resolvePath(fileSystem, UFS_ROOT_DIRECTORY_INODE_NUMBER, parts);

  inode_t inode;
  if (fileSystem->stat(curInode, &inode) < 0)
    throw ClientError::notFound();

  // regular file
  if (inode.type == UFS_REGULAR_FILE) {
    string body;
    if (inode.size > 0) {
      unsigned char *buf = new unsigned char[inode.size];
      int bytesRead = fileSystem->read(curInode, buf, inode.size);
      if (bytesRead < 0) {
        delete[] buf;
        throw ClientError::badRequest();
      }
      body.assign(reinterpret_cast<char *>(buf), bytesRead);
      delete[] buf;
    }
    response->setBody(body);
    return;
  }

  // directory
  if (inode.type == UFS_DIRECTORY) {
    string body;
    if (inode.size > 0) {
      unsigned char *buf = new unsigned char[inode.size];
      int bytesRead = fileSystem->read(curInode, buf, inode.size);
      if (bytesRead < 0) {
        delete[] buf;
        throw ClientError::badRequest();
      }

      int numEntries = bytesRead / sizeof(dir_ent_t);
      dir_ent_t *entries = reinterpret_cast<dir_ent_t *>(buf);

      vector<string> listing;
      for (int i = 0; i < numEntries; ++i) {
        // skip deleted / empty slots.
        if (entries[i].inum < 0)
          continue;

        // safely copy the fixed-width name field
        char nameBuf[DIR_ENT_NAME_SIZE + 1];
        memcpy(nameBuf, entries[i].name, DIR_ENT_NAME_SIZE);
        nameBuf[DIR_ENT_NAME_SIZE] = '\0';
        string name(nameBuf);

        //skip . and ..
        if (name == "." || name == "..")
          continue;

        //append / to directories
        inode_t entrySt;
        fileSystem->stat(entries[i].inum, &entrySt);
        listing.push_back(entrySt.type == UFS_DIRECTORY ? name + "/" : name);
      }
      delete[] buf;

      sort(listing.begin(), listing.end());
      for (const string &e : listing)
        body += e + "\n";
    }
    response->setBody(body);
    return;
  }

  throw ClientError::notFound();
}

//put function

void DistributedFileSystemService::put(HTTPRequest *request, HTTPResponse *response)
{
  string path = request->getPath();

  // remove the ds3 prefix
  string fsPath = path.substr(5);

  vector<string> parts = splitPath(fsPath);

  // PUT must target a specific filename, not the root.
  if (parts.empty())
    throw ClientError::badRequest();

  // Split into directory components + final filename.
  string filename = parts.back();
  vector<string> dirParts(parts.begin(), parts.end() - 1);
  string body = request->getBody();

  int curInode = UFS_ROOT_DIRECTORY_INODE_NUMBER;

  // create each directory in the path
  for (const string &dir : dirParts) {
    int found = fileSystem->lookup(curInode, dir);

    if (found < 0) {
      found = fileSystem->create(curInode, UFS_DIRECTORY, dir);
      if (found < 0)
        throw ClientError::insufficientStorage();
    } else {
      inode_t st;
      fileSystem->stat(found, &st);
      if (st.type != UFS_DIRECTORY)
        throw ClientError::conflict();
    }
    curInode = found;
  }

  // resolve (or create) the target file
  int fileInode = fileSystem->lookup(curInode, filename);

  if (fileInode < 0) {
    fileInode = fileSystem->create(curInode, UFS_REGULAR_FILE, filename);
    if (fileInode < 0)
      throw ClientError::insufficientStorage();
  } else {
    inode_t st;
    fileSystem->stat(fileInode, &st);
    if (st.type != UFS_REGULAR_FILE)
      throw ClientError::conflict();
  }

  // Write (overwrite) the full file contents.
  int toWrite = static_cast<int>(body.size());
  int written = fileSystem->write(fileInode, body.c_str(), toWrite);
  if (toWrite > 0 && written < toWrite)
    throw ClientError::insufficientStorage();

  response->setBody("");
}

//delete function

void DistributedFileSystemService::del(HTTPRequest *request, HTTPResponse *response)
{
  string path = request->getPath();

  // remove the ds3 prefix
  string fsPath = path.substr(5);

  vector<string> parts = splitPath(fsPath);

  // root cannot be deleted, check for empty first
  if (parts.empty())
    throw ClientError::badRequest();

  string targetName = parts.back();
  vector<string> dirParts(parts.begin(), parts.end() - 1);

  // resolve parent directory
  int parentInode = UFS_ROOT_DIRECTORY_INODE_NUMBER;
  for (const string &dir : dirParts) {
    int found = fileSystem->lookup(parentInode, dir);
    if (found < 0)
      throw ClientError::notFound();

    inode_t st;
    fileSystem->stat(found, &st);
    if (st.type != UFS_DIRECTORY)
      throw ClientError::notFound();

    parentInode = found;
  }

  // confirm target exists
  int targetInode = fileSystem->lookup(parentInode, targetName);
  if (targetInode < 0)
    throw ClientError::notFound();

  // if inode is directory, verify if empty (only "." and ".." entries)
  inode_t targetSt;
  fileSystem->stat(targetInode, &targetSt);
  if (targetSt.type == UFS_DIRECTORY) {
    int numEntries = targetSt.size / (int)sizeof(dir_ent_t);
    if (numEntries > 2)
      throw ClientError::conflict(); // directory not empty
  }

  int result = fileSystem->unlink(parentInode, targetName);
  if (result < 0)
    throw ClientError::badRequest();

  response->setBody("");
}