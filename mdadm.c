#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

//Create global variable to check if mounted or not
int isMounted = 0;

uint32_t encode_op(int cmd, int disk_num, int block_num) {
  uint32_t op = 0;
  op |= (cmd << 26);
  op |= (disk_num << 22);
  op |= (block_num);
  return op;
}

int mdadm_mount(void) {
  //If it is already mounted return a failure
  if(isMounted == 1)
    return -1;
  uint32_t op = encode_op(JBOD_MOUNT, 0, 0);
  jbod_client_operation(op, NULL);
  //Change the global variable to it being mounted
  isMounted = 1;
  //Return success
  return 1;
}

int mdadm_unmount(void) {
  //If it is already unmounted return a failure
  if(isMounted == 0)
    return -1;
  uint32_t op = encode_op(JBOD_UNMOUNT, 0, 0);
  jbod_client_operation(op, NULL);
  //Chang the global variable to it being unmounted
  isMounted = 0;
  //Return success
  return 1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  //If it is unmounted, trying to read an out of bound address, or trying a read larger than 1024 bytes return a failure
  if(isMounted == 0 || JBOD_DISK_SIZE * JBOD_NUM_DISKS < len + addr || (buf == NULL && len > 0) || 1024 < len)
    return -1;

  //Create buffer variable
  uint8_t tmpBuf[256];

  //Translate the address into its variables
  int disk_num = addr / JBOD_DISK_SIZE;
  int block_num = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  int offset = (addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;

  //Find the number of blocks being read
  int blockAmount = (len + offset) / 256 + 1;
  //If the only one block is being read then set block amount to one
  if(len + offset <= 256) 
    blockAmount = 1;

  //Create buffer variable
  uint8_t myBuf[blockAmount * 256];
  
  for(int i = 0; i < blockAmount; ++i) {
    //Call seek function to seek to the disk number and block number
    seek(disk_num, block_num);

    //If the cache is not hit read from jbod
    if(cache_lookup(disk_num, block_num, myBuf) == -1) {
      jbod_client_operation(encode_op(JBOD_READ_BLOCK, 0, 0), tmpBuf);
      //Use cache insert to insert entry
      cache_insert(disk_num, block_num, tmpBuf);
    }

    //Move to the next block
    block_num += 1;
    //If the block is equal to the number of blocks in a disk
    if(block_num == JBOD_NUM_BLOCKS_PER_DISK) {
      //Move to next disk
      disk_num += 1;
      //Set block number to zero
      block_num = 0;
    }
    //Copy bytes into the buffer variable
    memcpy(&myBuf[i*256], tmpBuf, 256);
  }
  //Copy the bytes into the buf
  memcpy(buf, &myBuf[offset], len);
  return len;
}

int seek(int disk_num, int block_num) {
  //Seek to the disk
  jbod_client_operation(encode_op(JBOD_SEEK_TO_DISK, disk_num, 0), NULL);
  //Seek to the block
  jbod_client_operation(encode_op(JBOD_SEEK_TO_BLOCK, 0, block_num), NULL);
  return -1;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  //If it is unmounted, trying to write an out of bound address, or trying a write larger than 1024 bytes return a failure
  if(isMounted == 0 || JBOD_DISK_SIZE * JBOD_NUM_DISKS < len + addr || (buf == NULL && len > 0) || 1024 < len)
     return -1;
   
  int tmpOffset = 0;
  //Create variable to hold the address
  int tmpAddr = addr;
  //Create buffer variable
  uint8_t myBuf[256];
  
  while(len + addr > tmpAddr) {
    //Translate the address into its variables
    int disk_num = tmpAddr / JBOD_DISK_SIZE;
    int block_num = (tmpAddr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int offset = (tmpAddr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;

    //Call seek function to seek to the disk number and block number
    seek(disk_num, block_num);

    //If cache is not hit then read from jbod
    if(cache_lookup(disk_num, block_num, myBuf) == -1)
      //Read block
      jbod_client_operation(encode_op(JBOD_READ_BLOCK, 0, 0), myBuf);

    //Caclulate whats left to write
    int writeLeft = (len - tmpAddr) + addr;
    int currSize = writeLeft;

    //If the offset plus bytes left are more than a block set the current size to be a block and its offset less
    if(offset + writeLeft > 256)
      currSize = 256 - offset;

    //Copy the bytes
    memcpy(myBuf + offset, buf + tmpOffset, currSize);
    //Update the cache
    cache_update(disk_num, block_num, myBuf);
    //Call seek function to seek to the disk number and block number
    seek(disk_num, block_num);
    //Write block
    jbod_client_operation(encode_op(JBOD_WRITE_BLOCK, 0, 0), myBuf);
    
    //Increment the address and offset
    tmpAddr += currSize;
    tmpOffset += currSize;
  }
  return len;
}
