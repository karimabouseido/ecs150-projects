#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <iostream>
#include <map>
#include <string>

#include "FileService.h"
#include "dthread.h"

using namespace std;

FileService::FileService(string basedir) : HttpService("/") {
  while (endswith(basedir, "/")) {
    basedir = basedir.substr(0, basedir.length() - 1);
  }

  if (basedir.length() == 0) {
    cout << "invalid basedir" << endl;
    exit(1);
  }

  this->m_basedir = basedir;
}

FileService::~FileService(){}

bool FileService::endswith(string str, string suffix) {
  size_t pos = str.rfind(suffix);
  return pos == (str.length() - suffix.length());
}

void FileService::get(HTTPRequest *request, HTTPResponse *response) {
  string path = this->m_basedir + request->getPath();

  if (path.find("..") != string::npos) {
      //server must NOT access parent files outside of the basedir
      response->setStatus(403);
      return;
    }

  // Check if file exists and is readable using stat()
  struct stat st;
  if (stat(path.c_str(), &st) < 0) {
    // File access failed - check what error occurred
    if (errno == ENOENT) {
      response->setStatus(404); // Not found
    } else {
      response->setStatus(403); // Forbidden (permission denied or other error)
    }
    return;
  }

  string fileContents = this->readFile(path);

  if (fileContents.size() == 0) {
    response->setStatus(403); // File exists but couldn't read (permission issue)
    return;
  } else {
    if (this->endswith(path, ".css")) {
      response->setContentType("text/css");
    } else if (this->endswith(path, ".js")) {
      response->setContentType("text/javascript");
    }
    response->setBody(fileContents);
  }
}

string FileService::readFile(string path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return "";
  }

  string result;
  int ret;
  char buffer[4096];
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    result.append(buffer, ret);
  }

  close(fd);
  return result;
}

void FileService::head(HTTPRequest *request, HTTPResponse *response) {
  // HEAD is the same as get but with no body
  this->get(request, response);
  response->setBody("");
}
