////////////////////////////////////////////////////////////////////////////////
//
//  File           : lcloud_filesys.c
//  Description    : This is the implementation of the Lion Cloud device 
//                   filesystem interfaces.
//
//   Author        : *** Sung Woo Oh ***
//   Last Modified : *** 2/26/2020 ***
//

// Include files
#include <stdlib.h>
#include <string.h>
#include <cmpsc311_log.h>

// Project include files
#include <lcloud_filesys.h>
#include <lcloud_controller.h>

//bool typedef
typedef int bool;
#define true 1
#define false 0


static LCloudRegisterFrame frm, rfrm, b0, b1, c0, c1, c2, d0, d1;
LCloudRegisterFrame nextd0;
int numdevice; //number of devices // there are 5 devices in assign3
#define filenum 100


//LcDeviceId did;
bool isDeviceOn;
bool secblk[10][64];

typedef struct{
    char *fname;
    LcFHandle fhandle;
    bool isopen;
    uint32_t pos;
    int flength;

}filesys;

typedef struct{
    LcDeviceId did;
    int blk;
    int sec;
    filesys finfo[filenum]; //file structure

}device;
device *devinfo;




////////////////////////////////////////////////////////////////////////////////
//
// Function     : getfreeblk
// Description  : iterate the storage(2d array) and find the free sector(i)&block(j) to read/write

void getfreeblk(){
    int i,j;
    for(i=0; i<10; i++){
        for(j=0; j<64; j++){
            if(secblk[i][j] == 0){
                devinfo->sec = i;
                devinfo->blk = j;
                return ;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_lcoud_registers
// Description  : create a register structure by packing the values using bit operations
//
// Inputs       : b0, b1, c0, c1, c2, d0, d1
// Outputs      : packedregs

LCloudRegisterFrame create_lcloud_registers(uint64_t cb0, uint64_t cb1, uint64_t cc0, uint64_t cc1, uint64_t cc2, uint64_t cd0, uint64_t cd1){
    uint64_t packedregs = 0x0, tempb0, tempb1, tempc0, tempc1, tempc2, tempd0, tempd1;

    tempb0 = (cb0 & 0xffff) << 60;   // 4 bit
    tempb1 = (cb1 & 0xffff) << 56;   // 4 bit
    tempc0 = (cc0 & 0xffff) << 48;   // 8 bit
    tempc1 = (cc1 & 0xffff) << 40;   // 8 bit
    tempc2 = (cc2 & 0xffff) << 32;   // 8 bit 
    tempd0 = (cd0 & 0xffff) << 16;   // 16 bit
    tempd1 = (cd1 & 0xffff);         // 16 bi

    packedregs = tempb0|tempb1|tempc0|tempc1|tempc2|tempd0|tempd1;  //packed
    return packedregs;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_lcloud_registers
// Description  : unpack the registers created with previous function(resp)
//
// Inputs       : resp, *b0, *b1, *c0, *c1, *c2, *d0, *d1

uint64_t extract_lcloud_registers(LCloudRegisterFrame resp, uint64_t *b0, uint64_t *b1, uint64_t *c0, uint64_t *c1, uint64_t *c2, uint64_t *d0, uint64_t *d1 ){
    *b0 = (resp >> 60) & 0xf;
    *b1 = (resp >> 56) & 0xf;
    *c0 = (resp >> 48) & 0xff;
    *c1 = (resp >> 40) & 0xff;
    *c2 = (resp >> 32) & 0xff;
    *d0 = (resp >> 16) & 0xffff;
    *d1 = (resp & 0xffff);
    
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : probeID
// Description  : check the device ID
//
// Inputs       : id_c1

uint8_t probeID(uint16_t id0){
    unsigned int temp = id0;
    unsigned int leastbit = id0 & ~(id0-1);

    //id_d0 = (id_d0 & 0xff);
    unsigned count = 0;
    // increment count until id_c1 = 0
    while(id0){
        id0 = id0 & ~(id0-1);
        id0 >>= 1;
        count++;
    }

    d0 = temp - leastbit;


    return count-1;  //shifted amount -1 will be device id
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : do_read
//
// Input        : did, sec, blk, *buf
//
// Description  : create the registers, call the io-bus, take the 64-bit value and back,
//                extract the registers, and check value 0.
//

int do_read(int did, int sec, int blk, char *buf){

    frm = create_lcloud_registers(0, 0 ,LC_BLOCK_XFER ,did, LC_XFER_READ, sec, blk); 

    if( (frm == -1) || ((rfrm = lcloud_io_bus(frm, buf)) == -1) || 
    (extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) || (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER)){
        logMessage(LOG_ERROR_LEVEL, "LC failure reading blkc [%d/%d/%d].", did, sec, blk);
        return(-1);
    }
    logMessage(LcDriverLLevel, "LC success reading blkc [%d/%d/%d].", did, sec, blk);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : do_write
//
// Input        : did, sec, blk, *buf
//
// Description  : create the registers, call the io-bus, take the 64-bit value and back,
//                extract the registers, and check value 0.
//

int do_write(int did, int sec, int blk, char *buf){

    frm = create_lcloud_registers(0, 0 ,LC_BLOCK_XFER ,did, LC_XFER_WRITE, sec, blk);  

    if( (frm == -1) || ((rfrm = lcloud_io_bus(frm, buf)) == -1) ||   
    (extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1)) || (b0 != 1) || (b1 != 1) || (c0 != LC_BLOCK_XFER)){ 
        logMessage(LOG_ERROR_LEVEL, "LC failure writing blkc [%d/%d/%d].", did, sec, blk);
        return(-1);
    }
    logMessage(LcDriverLLevel, "LC success writing blkc [%d/%d/%d].", did, sec, blk);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcpoweron
//
// Description  : power on the device 
//                  
//                b0/b1 - which direction / return status
//                c0    - Operation code (poweron/off/probe/transfer)
//                c1    - device id
//                c2    - read(0)/write(1)
//                d0/d1 - sector/block
//

int32_t lcpoweron(void){
    int i,j;
    int fd;
    int reserved0;

    //devinfo = malloc(sizeof(device));

    logMessage(LcControllerLLevel, "Initialzing Lion Cloud system ...");

    // Do Operation - PowerOn
    frm = create_lcloud_registers(0, 0 ,LC_POWER_ON ,0, 0, 0, 0); 
    rfrm = lcloud_io_bus(frm, NULL);
    extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1);

    isDeviceOn = true;
    bool first = true; //check if is calling probeID first time

    int n=0; //devinit loop counter
    
    devinfo = malloc(sizeof(device));

    // Do Operation - Devprobe
    frm = create_lcloud_registers(0, 0 ,LC_DEVPROBE ,0, 0, 0, 0); 
    rfrm = lcloud_io_bus(frm, NULL);
    extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1); //after extract I get probed d0 (22048)

    do{ //find out each multiple devices' number
        if(first == true){
            devinfo->did = probeID(d0);
            first = false;
        }
        else{
            devinfo->did = probeID(reserved0); 
        }
        //logMessage(LcControllerLLevel, "Found device [%d] in cloud probe.", devinfo->did);

        frm = create_lcloud_registers(0, 0 ,LC_DEVINIT ,devinfo->did, 0, 0, 0); 
        rfrm = lcloud_io_bus(frm, NULL);
        reserved0 = d0; // reserve d0 after probeID function
        extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1);
        logMessage(LcControllerLLevel, "Found device [did=%d, secs=%d, blks=%d] in cloud probe.", devinfo->did, d0, d1);

        n++;
    }while(n<5);


    ////////////////// initialize //////////////////////
    for(fd=0; fd<filenum; fd++){

        devinfo->finfo[fd].isopen = false;
        devinfo->finfo[fd].fname = "\0";   // ' '?
        devinfo->finfo[fd].pos = -1;
        devinfo->finfo[fd].fhandle = -1;
        devinfo->finfo[fd].flength = -1;
        //sector and block 

    }
    //initiailize secblk
    for(i=0;i<10;i++){
        for(j=0;j<64;j++){
            secblk[i][j] = 0;
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function     : newfilehandle
// Description  : create new filehandle.

int newfilehandle(){
    static int handle;
    handle += 1;
    return handle;
}

// File system interface implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcopen
// Description  : Open the file for for reading and writing
//
// Inputs       : path - the path/filename of the file to be read
// Outputs      : file handle if successful test, -1 if failure

LcFHandle lcopen( const char *path ) {

    int fd=0;

    //check if power is off, and poweron
    if(isDeviceOn == false){
        lcpoweron();
    }

    while(fd < filenum){
        //check if opening the file again
        if(strcmp(path, devinfo->finfo[fd].fname) == 0){
            if(devinfo->finfo[fd].isopen == true){
                logMessage(LOG_ERROR_LEVEL, "File is already opened.\n\n");
                return -1;
            }
        }
        //if we are opening another file, increment the file handle
        else{ 
            fd++;
            if(devinfo->finfo[fd].isopen == false ) break;
        }
    }

    devinfo->finfo[fd].isopen = true;
    devinfo->finfo[fd].fname = strdup(path);        //save file name
    devinfo->finfo[fd].fhandle = fd;                //pick unique file handle
    devinfo->finfo[fd].pos = 0;                     //set file pointer to first byte
    devinfo->finfo[fd].flength = 0;


    logMessage(LcControllerLLevel, "Opened new file [%s], fh=%d.", devinfo->finfo[fd].fname, devinfo->finfo[fd].fhandle);

    return(devinfo->finfo[fd].fhandle);
} 

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcread
// Description  : Read data from the file 
//
// Inputs       : fh - file handle for the file to read from
//                buf - place to put the data
//                len - the length of the read
// Outputs      : number of bytes read, -1 if failure

int lcread( LcFHandle fh, char *buf, size_t len ) {

    uint32_t readbytes, filepos, blknum, secnum;
    uint16_t offset, remaining, size;
    char tempbuf[LC_DEVICE_BLOCK_SIZE];

    memset(tempbuf, 0x0, LC_DEVICE_BLOCK_SIZE);
    
    /*************Error Checking****************/

    //check if file handle is valid (is associated with open file)
    if(fh < 0 || devinfo->finfo[fh].isopen == false){
        logMessage(LOG_ERROR_LEVEL, "Failed to read: file handle is not valid or file is not opened");
        return -1;
    }
    //check length to if it is valid
    if(len < 0){
        logMessage(LOG_ERROR_LEVEL, "Failed to read: length is not valid");
        return -1;
    }
    //check if reading exceeds end of the file
    if(devinfo->finfo[fh].pos+len > devinfo->finfo[fh].flength){
        logMessage(LOG_ERROR_LEVEL, "Reading exceeds end of the file");
        return -1;
    }

    int start = devinfo->finfo[fh].flength - devinfo->finfo[fh].pos;
    if(len > start){
        len = start;
    }

    filepos = devinfo->finfo[fh].pos;
    readbytes = len;

    /////////////// begin reading ////////////////////

    while( readbytes > 0){
        //memset(tempbuf, 0x0, LC_DEVICE_BLOCK_SIZE);
        
        blknum = filepos/LC_DEVICE_BLOCK_SIZE;     // len/256
        offset = filepos % LC_DEVICE_BLOCK_SIZE;  //e.g. 50%256 = 50,  500%256 = 244 (1block and 244bytes)
        remaining = LC_DEVICE_BLOCK_SIZE - offset;  //e.g. 256-(500%256) = 12
        secnum = filepos/ (LC_DEVICE_BLOCK_SIZE * 10); //filepos/2304


        //if exceeds the len we will write will be the remaining
        if(readbytes < remaining){
            size = readbytes;
        }
        else{
            size = remaining;
        }
        
        devinfo->blk = blknum % 10;
        devinfo->sec = secnum;

        // read, and copy up to len to the buf
        do_read(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
        memcpy(buf, tempbuf+offset, size);


        /////// update position, readbytes, and buf offset //////
        filepos += size;
        readbytes -= size;
        buf += size;

        
        // if position exceeds the size of the file then increase file size to current position
        if(filepos > devinfo->finfo[fh].flength){
            devinfo->finfo[fh].flength = filepos;
        }

        devinfo->finfo[fh].pos = filepos;
    }

    logMessage(LcDriverLLevel, "Driver read %d bytes to file %s", len, devinfo->finfo[fh].fname, devinfo->finfo[fh].flength);
    return( len );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcwrite
// Description  : write data to the file
//
// Inputs       : fh - file handle for the file to write to
//                buf - pointer to data to write
//                len - the length of the write
// Outputs      : number of bytes written if successful test, -1 if failure

int lcwrite( LcFHandle fh, char *buf, size_t len ) {

    uint32_t writebytes, filepos, blknum, secnum;
    uint16_t offset, remaining, size;
    char tempbuf[LC_DEVICE_BLOCK_SIZE];


    /*************Error Checking****************/
    
    //check if file handle is valid (is associated with open file)
    if(devinfo->finfo[fh].fhandle != fh || fh < 0 || devinfo->finfo[fh].isopen == false){
        logMessage(LOG_ERROR_LEVEL, "Failed to write: file handle is not valid or file is not opened");
        return -1;
    }
    //check length to if it is valid
    if(len < 0){
        logMessage(LOG_ERROR_LEVEL, "Failed to write: length is not valid");
        return -1;
    }
    
    /*************Begin Writing****************/
    writebytes = len;
    filepos = devinfo->finfo[fh].pos;


    while(writebytes > 0){

        blknum = filepos/LC_DEVICE_BLOCK_SIZE;     // len/256
        offset = filepos % LC_DEVICE_BLOCK_SIZE;  //e.g. 50%256 = 50,  500%256 = 244 (1block and 244bytes)
        remaining = LC_DEVICE_BLOCK_SIZE - offset;  //e.g. 256-(500%256) = 12
        secnum = filepos/ (LC_DEVICE_BLOCK_SIZE * 10); //filepos/2304


        ////////////////////// Sector and Block ///////////////////////////////
        // mark off used sectors or block
        if(devinfo->blk == (blknum % 10)){
            secblk[devinfo->sec][devinfo->blk] = 1;
        }
        getfreeblk();
        if(devinfo->blk > 63){
            logMessage(LOG_ERROR_LEVEL, "Block number exceeds 64");
            return -1;
        }

        devinfo->blk = blknum % 10;
        devinfo->sec = secnum;
        //////////////////// Sector and Block end /////////////////////////////
        

        //if exceeds the len we will write will be the remaining
        if(writebytes < remaining){
            size = writebytes;
        }
        else{
            size = remaining;
        }

        /************** if fills exactly 256  *****************/
        if(offset + writebytes == LC_DEVICE_BLOCK_SIZE){
            do_read(devinfo->did, devinfo->sec, devinfo->blk, tempbuf); //read to find offset
            memcpy(tempbuf+offset, buf, writebytes );
            do_write(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
        }
        /////////// one block done ///////////


        /************* if exceeds the block size **************/
        else if(offset + writebytes > LC_DEVICE_BLOCK_SIZE){ 
            //write number of blocks
            do_read(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
            memcpy(tempbuf+offset, buf, size );
            do_write(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
            /////////// one block done ///////////
        }

        else{  //if(offset + len < 256)
            do_read(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
            memcpy(tempbuf+offset, buf, size ); //flength%256 instead of size?
            do_write(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
            // fill with garbage??
        }


        ////////update pos, decrease len used (bytesleft to write), update buffer after written///////////////////
        filepos += size; 
        writebytes -= size;
        buf += size;


        // if position exceeds the size of the file then increase file size to current position
        if(filepos > devinfo->finfo[fh].flength){
            devinfo->finfo[fh].flength = filepos;
        }
      
        devinfo->finfo[fh].pos = filepos;
    }
    
    logMessage(LcDriverLLevel, "Driver wrote %d bytes to file %s (now %d bytes)", len, devinfo->finfo[fh].fname, devinfo->finfo[fh].flength);
    return( len );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcseek
// Description  : Seek to a specific place in the file
//
// Inputs       : fh - the file handle of the file to seek in
//                off - offset within the file to seek to
// Outputs      : 0 if successful test, -1 if failure

int lcseek( LcFHandle fh, size_t off ) {
    //filesys *devinfo.finfo;

    if(fh < 0 || devinfo->finfo[fh].isopen == false || isDeviceOn == false || (devinfo->finfo[fh].pos + off) > devinfo->finfo[fh].flength || devinfo->finfo[fh].flength < 0){
        logMessage(LOG_ERROR_LEVEL, "file failed to seek in");
        return -1;
    }
    if(devinfo->finfo[fh].flength < off){
        logMessage(LOG_ERROR_LEVEL, "Seeking out of file [%d < %d]", devinfo->finfo[fh].flength, off);
    }

    logMessage(LcDriverLLevel, "Seeking to position %d in file handle %d [%s]", off, fh, devinfo->finfo[fh].fname);
    devinfo->finfo[fh].pos = off;

    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcclose
// Description  : Close the file
//
// Inputs       : fh - the file handle of the file to close
// Outputs      : 0 if successful test, -1 if failure

int lcclose( LcFHandle fh ) {
    
    //check if there is no file to close
    if(devinfo->finfo[fh].isopen == false){
        logMessage(LOG_ERROR_LEVEL, "There is no opened file to close");
        return -1;
    }
    //free if malloc

    //close file
    devinfo->finfo[fh].isopen = false;

    logMessage(LcDriverLLevel, "Closed file handle %d [%s]", fh, devinfo->finfo[fh].fname);
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : lcshutdown
// Description  : Shut down the filesystem
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure

int lcshutdown( void ) {

    //Poweroff
    frm = create_lcloud_registers(0, 0 ,LC_POWER_OFF ,0, 0, 0, 0); 
    lcloud_io_bus(frm, NULL);
    logMessage(LcDriverLLevel, "Powered off the Lion cloud system.");

    isDeviceOn = false;

    return( 0 );
}
