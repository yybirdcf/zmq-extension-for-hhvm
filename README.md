##zmq-extension-for-hhvm
======================

A simple zmq extension implement for hhvm, a hard try!!!

###Environment

The extension is developed at ubuntu12.04 and gcc version 4.8.1

###Installing

* First, you need install hhvm : [HHVM](https://github.com/facebook/hhvm)

* Second, you need install libzmq : [LIBZMQ](https://github.com/zeromq/libzmq)

* Then, sh build.sh or chmod +x build.sh and ./build.sh

* If everything is ok, you can see zmq.so in the folder, now add 

```
DynamicExtensions {
  example = /path/to/zmq.so
}
```
to your hhvm config.hdf

###Report Errors
First, I am sorry about anything unexpected! If you get any trouble when installing and running the extension , please tell me (haipengchencf@gmail.com); 