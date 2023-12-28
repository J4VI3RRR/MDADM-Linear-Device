#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  //Counter to hold number of bytes read
  int num_read = 0;
  while(num_read < len) {
    //Read bytes
    int n = read(fd, &buf[num_read], len - num_read);
    //If n is negative or zero then it is a failure
    if(n <= 0)
      return false;
    //Increment the number of bytes read
    num_read += n;
  }
  //Failure never returned so is a success
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  //Counter to hold number of bytes written
  int num_written = 0;
  while(num_written < len) {
    //Write bytes
    int n = write(fd, &buf[num_written], len - num_written);
    //If n is negative or zero then it is a failure
    if(n <= 0)
      return false;
    //Increment the number of bytes written
    num_written += n;
  }
  //Failure never returned so is a success
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t header[HEADER_LEN];

  //If read fails return that recieving packet failed
  if(nread(fd, HEADER_LEN, header) == false)
    return false;

  //Update the opcode of packet
  memcpy(op, header + 2, 4);
  *op = htonl(*op);

  //Update info code of packet
  memcpy(ret, header + 6, 2);
  *ret = htons(*ret);

  //Update the length of packet
  uint16_t len;
  memcpy(&len, header, 2);
  len = htons(len);

  //If a block plus the length of header are equal to length
  if (256 + HEADER_LEN == len)
    //If read fails return that recieving packet failed
    if(nread(fd, 256, block) == false)
      return false;

  //Packet successfully recieved
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t header[HEADER_LEN + 256];
  uint16_t len = HEADER_LEN;
  //Translate to network length
  uint16_t netLen = htons(len);
  //Translate to network opcode
  uint32_t netOp = htonl(op);

  //Update header with new op code
  memcpy(header + 2, &netOp, 4);
  
  if ((op>>26) == JBOD_WRITE_BLOCK) {
    //Add a block to length
    len = HEADER_LEN + 256;
    //Translate to network length
    netLen = htons(len);
    //Update header with new block
    memcpy(header + 8, block, 256);
  }
  //Update header with new length
  memcpy(header, &netLen, 2);

  //If write fails return that sending a packet failed
  if(nwrite(sd, len, header) == false)
     return false;

  //Packet successfully sent
  return true;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
  //Create the socket
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);

  //If socket creation failed return connection failed
  if(cli_sd == -1)
    return false;

  //Address of server
  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);

  //If string is invalid to be converted to binary return failure
  if(inet_aton(ip, &caddr.sin_addr) == 0)
    return false;
  //If failed to connect return failure
  else if(connect(cli_sd, (const struct sockaddr*)&caddr, sizeof(caddr)) == -1)
    return false;

  //Connection was successful
  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  //Disconnect from server
  close(cli_sd);
  //Reset cli_sd
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  //Write the packet
  send_packet(cli_sd, op, block);
  //Recieve the packet
  uint16_t ret;
  recv_packet(cli_sd, &op, &ret, block);
  return ret;
}
