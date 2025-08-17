Sockets come in two basic types—connection-oriented and connectionless

The two protocols that are used today are Transmission Control Protocol (TCP) and User Datagram Protocol (UDP). TCP is a connection-oriented protocol, and UDP is a connectionless protocol.

## TCP program flow

### Client

Given hostname and port
- resolve address
- create a socket
- establishe connection
- (exchange using send() and recv())

### Server
- initialize address
- create a socket
- bind socket to the listening IP addr and port
- put socket in listening state
- wait until a client establishes a connection, once connected returns a new socket for exchanging data
- (exchange using send() and recv())

## UDP program flow

### Client
- resolve address
- creates a socket
- send first packet
- continue sending

### Server
- initialize address
- creates a socket
- bind the socket
- block until receive data from a client
