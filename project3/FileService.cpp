#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

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
  this->m_last_error = 0;
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

  string fileContents = this->readFile(path);

  if (fileContents.size() == 0) {
    // Distinguish between 404 (not found) and 403 (forbidden)
    if (m_last_error == ENOENT) {
      response->setStatus(404);
    } else {
      response->setStatus(403);
    }
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
    m_last_error = errno;
    return "";
  }

  string result;
  int ret;
  char buffer[4096];
  while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
    result.append(buffer, ret);
  }

  close(fd);
  m_last_error = 0;
  return result;
}

void FileService::head(HTTPRequest *request, HTTPResponse *response) {
  // HEAD is the same as get but with no body
  this->get(request, response);
  response->setBody("");
}
