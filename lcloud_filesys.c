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
#define filenum 33
#define devicenum 5


//LcDeviceId did;
bool isDeviceOn;
bool secblk[10][64];

typedef struct{
    char *fname;
    LcFHandle fhandle;
    bool isopen;
    uint32_t pos;
    int flength;
    //device info <-> file 
    int fnow;
    LcDeviceId fdid;
    int fsec;
    int fblk;


}filesys;
filesys finfo[filenum]; //file structure

typedef struct{
    LcDeviceId did;
    int sec;
    int blk;
    char **storage;
    int maxsec;
    int maxblk;
    int devwritten;
    int devread;
    
}device;
device *devinfo;
int allocatedblock = 0; // number of blocks allocated
int totalblock = 0; // total number of blocks calculated during allocation
int now = 0; // current device id
int prevfilesnow = 0; //previous file's device id
bool findnextfreedev;


////////////////////////////////////////////////////////////////////////////////
//
// Function     : getfreeblk
// Description  : iterate the storage(2d array) and find the free sector(i)&block(j) to read/write

void getfreeblk(int n, LcFHandle fh){ //argument = now 
    int i,j;

    for(i=0; i<devinfo[n].maxsec; i++){
        for(j=0; j<devinfo[n].maxblk; j++){
            if(devinfo[n].storage[i][j] == 0){
                devinfo[n].sec = i;
                devinfo[n].blk = j;
                now = n;
                return ;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : nextdevice
// Description  : move onto next device

void nextdevice(int *n){
    if(*n>=devicenum-1){
        *n=0;
    }
    else{
        (*n)++;
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
    
    devinfo = malloc(sizeof(device) * devicenum);

    // Do Operation - Devprobe
    frm = create_lcloud_registers(0, 0 ,LC_DEVPROBE ,0, 0, 0, 0); 
    rfrm = lcloud_io_bus(frm, NULL);
    extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1); //after extract I get probed d0 (22048)

    //---------------------- Device init ----------------------------//
    do{ //find out each multiple devices' number
    
        if(first == true){
            devinfo[n].did = probeID(d0);
            first = false;
        }
        else{
            devinfo[n].did = probeID(reserved0); 
        }
        //logMessage(LcControllerLLevel, "Found device [%d] in cloud probe.", devinfo->did);

        frm = create_lcloud_registers(0, 0 ,LC_DEVINIT ,devinfo[n].did, 0, 0, 0); 
        rfrm = lcloud_io_bus(frm, NULL);
        reserved0 = d0; // reserve d0 after probeID function
        extract_lcloud_registers(rfrm, &b0, &b1, &c0, &c1, &c2, &d0, &d1);
        devinfo[n].maxsec = d0;
        devinfo[n].maxblk = d1;
        logMessage(LcControllerLLevel, "Found device [did=%d, secs=%d, blks=%d] in cloud probe.", devinfo[n].did, d0, d1);

        

        //------------device tracker allocation ----------//
        devinfo[n].storage = (char **) malloc(sizeof(char*) * devinfo[n].maxsec); //ex. did = 5,  blk = 64
        for(i=0; i<devinfo[n].maxsec; i++){
            devinfo[n].storage[i] = (char *) malloc(sizeof(char) * devinfo[n].maxblk);  //ex. did = 5. sec = 10
        }
        // zero out storage (device tracker)
        for(i=0; i<devinfo[n].maxsec; i++){
            for(j=0; j< devinfo[n].maxblk; j++){
                devinfo[n].storage[i][j] = 0;
            }
        }
        /////////////////////////////////////////////////////
        totalblock += devinfo[n].maxsec * devinfo[n].maxblk;

        
        devinfo[n].blk = 0;
        devinfo[n].sec = 0;
        devinfo[n].devwritten =0;
        devinfo[n].devread =0;
        
        //increment index(next device)
        n++;
    }while(n<devicenum);

    ////////////////// file initialize //////////////////////
    for(fd=0; fd<filenum; fd++){

        finfo[fd].isopen = false;
        finfo[fd].fname = "\0";   // ' '?
        finfo[fd].pos = -1;
        finfo[fd].fhandle = -1;
        finfo[fd].flength = -1;
        //device <-> file
        finfo[fd].fdid = -1;
        finfo[fd].fsec = -1;
        finfo[fd].fblk = -1;
        finfo[fd].fnow = -1;
    }


    return 0;
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
        if(strcmp(path, finfo[fd].fname) == 0){
            if(finfo[fd].isopen == true){
                logMessage(LOG_ERROR_LEVEL, "File is already opened.\n\n");
                return -1;
            }
        }
        //if we are opening another file, increment the file handle
        else{ 
            fd++;
            if(finfo[fd].isopen == false ) break;
        }
    }

    finfo[fd].isopen = true;
    finfo[fd].fname = strdup(path);        //save file name
    finfo[fd].fhandle = fd;                //pick unique file handle
    finfo[fd].pos = 0;                     //set file pointer to first byte
    finfo[fd].flength = 0;
    //device <-> file
    finfo[fd].fdid = 0;
    finfo[fd].fsec = 0;
    finfo[fd].fblk = 0;
    finfo[fd].fnow = 0;


    logMessage(LcControllerLLevel, "Opened new file [%s], fh=%d.", finfo[fd].fname, finfo[fd].fhandle);

    return(finfo[fd].fhandle);
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
    if(fh < 0 || finfo[fh].isopen == false){
        logMessage(LOG_ERROR_LEVEL, "Failed to read: file handle is not valid or file is not opened");
        return -1;
    }
    //check length to if it is valid
    if(len < 0){
        logMessage(LOG_ERROR_LEVEL, "Failed to read: length is not valid");
        return -1;
    }
    //check if reading exceeds end of the file
    if(finfo[fh].pos+len > finfo[fh].flength){
        logMessage(LOG_ERROR_LEVEL, "Reading exceeds end of the file");
        return -1;
    }

    int start = finfo[fh].flength - finfo[fh].pos;
    if(len > start){
        len = start;
    }

    filepos = finfo[fh].pos;
    readbytes = len;

    /////////////// begin reading ////////////////////

    while( readbytes > 0){
        //memset(tempbuf, 0x0, LC_DEVICE_BLOCK_SIZE);
        
        blknum = (devinfo->devread /LC_DEVICE_BLOCK_SIZE) % devinfo->maxblk;     // len/256
        offset = devinfo->devread % LC_DEVICE_BLOCK_SIZE;  //e.g. 50%256 = 50,  500%256 = 244 (1block and 244bytes)
        remaining = LC_DEVICE_BLOCK_SIZE - offset;  //e.g. 256-(500%256) = 12
        secnum = (devinfo->devread / (LC_DEVICE_BLOCK_SIZE * devinfo->maxblk)) % devinfo->maxsec; //filepos/2304


        //if exceeds the len we will write will be the remaining
        if(readbytes < remaining){
            size = readbytes;
        }
        else{
            size = remaining;
        }
        
        devinfo->blk = blknum % devinfo->maxblk;
        devinfo->sec = secnum;

        // read, and copy up to len to the buf
        do_read(devinfo->did, devinfo->sec, devinfo->blk, tempbuf);
        memcpy(buf, tempbuf+offset, size);


        /////// update position, readbytes, and buf offset //////
        filepos += size;
        readbytes -= size;
        buf += size;
        devinfo->devread += size;

        
        // if position exceeds the size of the file then increase file size to current position
        if(filepos > finfo[fh].flength){
            finfo[fh].flength = filepos;
        }

        finfo[fh].pos = filepos;
    }

    logMessage(LcDriverLLevel, "Driver read %d bytes to file %s", len, finfo[fh].fname, finfo[fh].flength);
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

    uint64_t writebytes, filepos;
    //uint64_t blknum, secnum;
    uint16_t offset, remaining, size;
    char tempbuf[LC_DEVICE_BLOCK_SIZE];
    prevfilesnow = now; // save last block point
    


    /*************Error Checking****************/
    
    //check if file handle is valid (is associated with open file)
    if(finfo[fh].fhandle != fh || fh < 0 || finfo[fh].isopen == false){
        logMessage(LOG_ERROR_LEVEL, "Failed to write: file handle is not valid or file is not opened");
        return -1;
    }
    //check length to if it is valid
    if(len < 0){
        logMessage(LOG_ERROR_LEVEL, "Failed to write: length is not valid");
        return -1;
    }
    
    /*************Begin Writing****************/
    writebytes = len;;
    filepos = finfo[fh].pos;


    while(writebytes > 0){

        getfreeblk(now, fh);

        // if there's block has some space left, go back to device that file lastly wrote
        if(devinfo[finfo[fh].fnow].storage[finfo[fh].fsec][finfo[fh].fblk] == 1){
            now = finfo[fh].fnow;
            findnextfreedev = true;
        }

        //allocate block
        
        if(devinfo[now].storage[devinfo[now].sec][devinfo[now].blk] < 1){
            devinfo[now].storage[devinfo[now].sec][devinfo[now].blk] = 1;
            allocatedblock++;
            logMessage(LOG_INFO_LEVEL, "Allocated block %d out of %d (%0.2f%%)", allocatedblock, totalblock, allocatedblock/totalblock);
            logMessage(LcDriverLLevel, "Allocated block for data [%d/%d/%d]", devinfo[now].did, devinfo[now].sec, devinfo[now].blk);
            finfo[fh].fdid = devinfo[now].did;
            finfo[fh].fsec = devinfo[now].sec;
            finfo[fh].fblk = devinfo[now].blk;
        } 
        
        //blknum = (devinfo[now].devwritten /LC_DEVICE_BLOCK_SIZE) % devinfo[now].maxblk;     // len/256
        offset = filepos % LC_DEVICE_BLOCK_SIZE;  //e.g. 50%256 = 50,  500%256 = 244 (1block and 244bytes)
        remaining = LC_DEVICE_BLOCK_SIZE - offset;  //e.g. 256-(500%256) = 12
        //secnum = (devinfo[now].devwritten / (LC_DEVICE_BLOCK_SIZE * devinfo[now].maxblk)) % devinfo[now].maxsec; //filepos/2304


        ////////////////////// Sector and Block ///////////////////////////////

        if(devinfo[now].blk > devinfo[now].maxblk){
            logMessage(LOG_ERROR_LEVEL, "Block number exceeds memory");
            return -1;
        }

        // devinfo[now].blk = blknum % devinfo[now].maxblk;
        // devinfo[now].sec = secnum;



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
            do_read(finfo[fh].fdid, finfo[fh].fsec, finfo[fh].fblk, tempbuf); //read to find offset
            memcpy(tempbuf+offset, buf, writebytes );
            do_write(finfo[fh].fdid, finfo[fh].fsec, finfo[fh].fblk, tempbuf);
            if(devinfo[now].sec != finfo[fh].fsec || devinfo[now].blk != finfo[fh].fblk){
                devinfo[now].storage[finfo[fh].fsec][finfo[fh].fblk] = 2;
            }
            else{
                devinfo[now].storage[devinfo[now].sec][devinfo[now].blk] = 2; // block is full
            }
        }
        /////////// one block done ///////////


        /************* if exceeds the block size **************/
        else if(offset + writebytes > LC_DEVICE_BLOCK_SIZE){ 
            //write number of blocks
            do_read(finfo[fh].fdid, finfo[fh].fsec, finfo[fh].fblk, tempbuf);
            memcpy(tempbuf+offset, buf, size );
            do_write(finfo[fh].fdid, finfo[fh].fsec, finfo[fh].fblk, tempbuf);
            if(devinfo[now].sec != finfo[fh].fsec || devinfo[now].blk != finfo[fh].fblk){
                devinfo[now].storage[finfo[fh].fsec][finfo[fh].fblk] = 2;
            }
            else{
                devinfo[now].storage[devinfo[now].sec][devinfo[now].blk] = 2; // block is full
            }
            /////////// one block done ///////////
        }

        else{  //if(offset + len < 256)
            do_read(finfo[fh].fdid, finfo[fh].fsec, finfo[fh].fblk, tempbuf);
            memcpy(tempbuf+offset, buf, size ); //flength%256 instead of size?
            do_write(finfo[fh].fdid, finfo[fh].fsec, finfo[fh].fblk, tempbuf);

        }





        ////////update pos, decrease len used (bytesleft to write), update buffer after written///////////////////
        filepos += size; 
        writebytes -= size;
        buf += size;
        devinfo[now].devwritten += size;
        finfo[fh].fnow = now;
        
        


        if(findnextfreedev == true){
            now = prevfilesnow;
        }
        else{
            nextdevice(&now);
        }
        
        


        // if position exceeds the size of the file then increase file size to current position
        if(filepos > finfo[fh].flength){
            finfo[fh].flength = filepos;
        }
      
        finfo[fh].pos = filepos;

    }
    findnextfreedev = false; //reset toggle
    logMessage(LcDriverLLevel, "Driver wrote %d bytes to file %s (now %d bytes)", len, finfo[fh].fname, finfo[fh].flength);
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
    //filesys finfo;

    if(fh < 0 || finfo[fh].isopen == false || isDeviceOn == false || finfo[fh].flength < 0 /*||(finfo[fh].pos + off) > finfo[fh].flength*/){
        logMessage(LOG_ERROR_LEVEL, "file failed to seek in");
        return -1;
    }
    if(finfo[fh].flength < off){
        logMessage(LOG_ERROR_LEVEL, "Seeking out of file [%d < %d]", finfo[fh].flength, off);
    }

    logMessage(LcDriverLLevel, "Seeking to position %d in file handle %d [%s]", off, fh, finfo[fh].fname);
    finfo[fh].pos = off;

    return( finfo[fh].pos ); //fix this 
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
    if(finfo[fh].isopen == false){
        logMessage(LOG_ERROR_LEVEL, "There is no opened file to close");
        return -1;
    }

    //close file
    finfo[fh].isopen = false;

    logMessage(LcDriverLLevel, "Closed file handle %d [%s]", fh, finfo[fh].fname);
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
    int i;

    //////////////////////// free //////////////////////////
    int n=0;
    do{
        for(i = 0; i < devinfo[n].maxblk; i++)
            free(devinfo[n].storage[i]);
        free(devinfo[n].storage);
    }while(n<devicenum);
    ////////////////////////////////////////////////////////


    //Poweroff
    frm = create_lcloud_registers(0, 0 ,LC_POWER_OFF ,0, 0, 0, 0); 
    lcloud_io_bus(frm, NULL);
    logMessage(LcDriverLLevel, "Powered off the Lion cloud system.");

    isDeviceOn = false;

    return( 0 );
}
