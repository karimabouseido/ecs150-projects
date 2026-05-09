#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <queue>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

vector<HttpService *> services;

queue<MySocket *> requestQueue;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER; //LOCK for threads
pthread_cond_t qNotEmpty = PTHREAD_COND_INITIALIZER; //monitor condition for queue not empty
pthread_cond_t qNotFull = PTHREAD_COND_INITIALIZER; //monitor condition for queue not full

HttpService *find_service(HTTPRequest *request) {
   // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}


void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL) {
    // not found status
    response->setStatus(404);
  } else if (request->isHead()) {
    service->head(request, response);
  } else if (request->isGet()) {
    service->get(request, response);
  } else {
    // The server doesn't know about this method
    response->setStatus(501);
  }
}

void handle_request(MySocket *client) {
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;
  
  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }    
    
  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }
  
  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());
    
  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

void *worker_thread(void *arg) {
  while (true) {
    MySocket *client;

    // get a request from the queue
    dthread_mutex_lock(&queueLock); // acquire lock
    while (requestQueue.size() == 0) { // thread waits if queue empty
      dthread_cond_wait(&qNotEmpty, &queueLock);
    }
    client = requestQueue.front(); // get client from front
    requestQueue.pop(); // remove from queue
    dthread_cond_signal(&qNotFull); // signal producer
    dthread_mutex_unlock(&queueLock); // release lock

    handle_request(client); // process request
  }
  return NULL;
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;

  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new FileService(BASEDIR));

  // THREAD POOL
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_t tid;
    dthread_create(&tid, NULL, worker_thread, NULL);
    dthread_detach(tid);
  }
  
  while(true) {
    sync_print("waiting_to_accept", "");
    client = server->accept();
    sync_print("client_accepted", "");
    
    // producer-consumer connection between the buffers and main thread
    dthread_mutex_lock(&queueLock); //LOCK for thread to access the queue
    //critical section for queue access
    while (requestQueue.size() >= (size_t)BUFFER_SIZE) { //if buffer is less than queue size, wait for signal where buffer is freed
      dthread_cond_wait(&qNotFull, &queueLock);
    }

    requestQueue.push(client); //push thread connection to queue in buffer slot
    dthread_cond_signal(&qNotEmpty); //signal next waiting thread
    dthread_mutex_unlock(&queueLock); //then unlock
  }
}
