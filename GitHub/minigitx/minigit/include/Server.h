#ifndef SERVER_H
#define SERVER_H

// Starts the local web interface (http://localhost:<port>) for the repository
// in the current folder. Blocks until the program is stopped with Ctrl+C.
int runServer(int port, bool openBrowser);

#endif
