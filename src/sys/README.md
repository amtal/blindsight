Only optional functionality thus far is file monitoring for reload. 

On Linux, this uses inotify. Could be implemented on OS X via FSEvents or a
common wrapper thereof, if needed.
