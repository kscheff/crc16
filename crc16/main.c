//  main.c
//  crc16
//
//  Created by Kai Scheffer on 18.12.16.
//  Copyright © 2016 Kai Scheffer. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

typedef struct {
  // Secure OAD uses the Signature for image validation instead of calculating a CRC, but the use
  // of CRC==CRC-Shadow for quick boot-up determination of a validated image is still used.
  uint16_t crc0;       // CRC must not be 0x0000 or 0xFFFF.
  uint16_t crc1;       // CRC-shadow must be 0xFFFF.
  // User-defined Image Version Number - default logic uses simple a '!=' comparison to start an OAD.
  uint16_t ver;
  uint16_t len;        // Image length in 4-byte blocks (i.e. HAL_FLASH_WORD_SIZE blocks).
  uint8_t  uid[4];     // User-defined Image Identification bytes.
  uint8_t  res[4];     // Reserved space for future use.
} img_hdr_t;


// calculate CRC-16 with init 0x0000, polynom 0x8005
// taken from <a href="https://www.lammertbies.nl/forum/viewtopic.php?t=1915">www.lammertbies.nl/.../viewtopic.php</a>
//
static uint16_t CRC16_BUYPASS(uint8_t *data, size_t len) {
  uint16_t crc = 0x0000;
  size_t j;
  int i;
  for (j=len; j>0; j--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (i=0; i<8; i++) {
      if (crc & 0x8000) crc = (crc<<1) ^ 0x8005;
      else crc <<= 1;
    }
  }
  return (crc);
}


int main(int argc, const char *argv[])
{
  if( argc < 2 || argc > 4 )
  {
    printf("Usage: %s input.bin [output.bin] \n\n", argv[0]);
    printf("Calculates and patches the CRC-16 for TI CC254x Firmware image.\n");
    printf("With no output file it prints only the old and new CRC.\n");
    printf("Typically the CRC is stored in the first 2 bytes following a 2 byte shaddow.\n");
    printf("CRC calulation starts with offset 4 and 2 bytes gets written at offset 0.\n\n");
    return 0;
  }
  
  FILE* fdat = fopen( argv[1], "rb" );
  if( !fdat ) {
    printf ("Error: Could not open input file '%s'.\n", argv[1]);
    return -1;
  }
  
  fseek(fdat, 0L, SEEK_END);
  size_t len = ftell(fdat);
  rewind(fdat);
  
  if (len < sizeof(img_hdr_t)) {
    printf("Error: File size too small!\n");
    return -1;
  }
  
  unsigned char *buf = malloc(len);
  if (buf == NULL) {
    printf("Error: Out of memory!\n");
    return -1;
  }
  
  if (len != fread(buf, 1, len, fdat)) {
    printf("Error: Could not read file!\n");
    return -1;
  }
  
  fclose( fdat );
  
  img_hdr_t *imgHdr = (img_hdr_t*) buf;
  
  printf("Image Header\n");
  printf("  crc0: %04X\n", imgHdr->crc0);
  printf("  crc1: %04X\n", imgHdr->crc1);
  printf("  ver : %04X\n", imgHdr->ver);
  printf("  len : %04X\n", imgHdr->len);
  printf("  uid : %02X %02X %02X %02X '%4.4s'\n", imgHdr->uid[0], imgHdr->uid[1], imgHdr->uid[2], imgHdr->uid[3], imgHdr->uid );
  printf("  res : %02X %02X %02X %02X\n", imgHdr->res[0], imgHdr->res[1], imgHdr->res[2], imgHdr->res[3]);

  if (imgHdr->len > len/4) {
    printf("Warning: File smaller than header len.\n");
  } else if (imgHdr->len != len / 4) {
    printf("Warning: File size and header len do not match.\n");
  }
  
  uint16_t crc = CRC16_BUYPASS(buf+4, len-4);
  
  printf("File length: %04X words.\n", (unsigned int)len/4);
  printf("  old CRC-16: %04X\n", (uint16_t) (buf[1])<<8 | buf[0]);
  printf("  new CRC-16: %04X\n", crc);
  
  if (argc < 3) {
    if  (crc != imgHdr->crc0) {
      printf("Fail: crc do not match.\n");
      return -1;
    } else if (len/4 != imgHdr->len) {
      printf("Fail: len do not match.\n");
      return -1;
    }
    printf("OK.\n");
  }
  
  if (argc == 3) {
    FILE* fout = fopen( argv[2], "wb" );
    if( !fout ) {
      printf ("Could not open output file '%s'.\n", argv[2]);
      return -1;
    } else {
      //patch new crc with LSB first
      buf[0] = crc & 0xff;
      buf[1] = crc>>8 & 0xff;
      //buf[2] = 0xff;
      //buf[3] = 0xff;
      
      if ( len != fwrite(buf, 1, len, fout) ) {
        printf("Could not write output file '%s'.\n", argv[2]);
        return -1;
      }
      fclose(fout);
    }
  }
  
  return 0;
}
