// web.h - the robot's web server (phone page + HTTP API).
#pragma once

void webInit();     // register the pages and start the server on port 80
void webUpdate();   // call every loop (answers waiting phone requests, never blocks long)
