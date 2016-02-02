#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#define RAM_START 0x0fff0000
#define RAM_START 0x10000000
#define READ_DATA 0x40004000
//#define RAM_START 0x1E000000
#define TCP_PORT 1002
#define SYSTEM_CALL_MAX 2
#define PKTZR_RESET_FLAG 1
#define WRITER_ENABLE_FLAG 2
#define UPDATE_CIC_FLAG 4
#define TRIGGER_RECORD_FLAG 8

#define RECORD_LENGTH_OFFSET 4
#define FREQ_OFFSET 8
#define DESIMATION_OFFSET  12
#define RECORD_START_POS_OFFSET 4
#define WRITER_STS_OFFSET 0
#define VALUE_OFFSET 16
#define RX_FIFO_CNT_OFFSET 24

int interrupted = 0;
int main(int argc, char *argv[])
{
  void *rx_data;
  int fd,fdio, i, sockServer,sockClient,yes = 1,samples,packet_size=4096, ch,temperature_raw, temperature_offset ;
  double temperature_scale,temperature;
  uint32_t status, trigger_pos, start_offset, end_offset, config;
  unsigned long size = 0, wait;
  uint64_t value;
  uint64_t command = 600000;
  void *cfg, *ram, *sts;
  char *name = "/dev/mem";
  char *system_call[] ={"cat /root/d.bit > /dev/xdevcfg","cat /root/fd.bit > /dev/xdevcfg"};
  struct sockaddr_in addr;
  size_t length;
  off_t offset=0x8000000;
  samples = 0x10000 * 1024 - 1;
  FILE *fp;
  int verbose=0;
  char buffer[49152];
  if (argc >=2 ) {
	  if (argv[1][0]=='v' ) verbose = 1;
	  if (argv[1][0]=='V' ) verbose = 2;
  }

  if((fd = open(name, O_RDWR)) < 0)
  {
    perror("open");
    return 1;
  }
  length=(samples+1)*4;
  cfg = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x40000000);
  ram = mmap(NULL,length , PROT_READ|PROT_WRITE, MAP_SHARED, fd, RAM_START);
  sts = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x40001000);
  rx_data = mmap(NULL, 64*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, READ_DATA);
  // reset writer
 //*((uint32_t *)(cfg + 0)) &= ~2;
 //*((uint32_t *)(cfg + 0)) |= 2;

  // enter reset mode for packetizer and fifo
  //*((uint32_t *)(cfg + 0)) &= ~5;
  //value = 50;
 *((uint32_t *)(cfg + 0)) &= ~0xf;
  //*((uint16_t *)(cfg + DESIMATION_OFFSET)) = 100;
  //*((uint32_t *)(cfg + 0)) |= UPDATE_CIC_FLAG;

  if((sockServer = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    perror("socket");
    return 1;
  }
  setsockopt(sockServer, SOL_SOCKET, SO_REUSEADDR, (void *)&yes , sizeof(yes));
  /* setup listening address */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(TCP_PORT);

  if(bind(sockServer, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("bind");
    return 1;
  }

  if((fp = fopen("/sys/bus/iio/devices/iio\:device0/in_temp0_offset", "r")) == NULL){
	    perror("device open");
	    return 1;
  }
  fscanf(fp,"%d", &temperature_offset);
  fclose(fp);
  if((fp = fopen("/sys/bus/iio/devices/iio\:device0/in_temp0_scale", "r")) == NULL){
  	    perror("device open");
  	    return 1;
    }
  fscanf(fp,"%lf", &temperature_scale);
  fclose(fp);
  if((fp = fopen("/sys/bus/iio/devices/iio\:device0/in_temp0_raw", "r")) == NULL){
  	    perror("device open");
  	    return 1;
    }
  fscanf(fp,"%d", &temperature_raw);
  fclose(fp);
  temperature=temperature_scale/1000*(temperature_raw+temperature_offset);
  if(verbose)printf("Temperature scale = %lf, offset = %d, raw = %d\nTemperature = %lf\n", temperature_scale, temperature_offset, temperature_raw, temperature);
  listen(sockServer, 1024);
  if(verbose)printf("waiting on client\n");
  if((sockClient = accept(sockServer, NULL, NULL)) < 0)
        {
          perror("accept");
          return 1;
        }
  if(verbose)printf("new connection\n");

  while(!interrupted)
      {
        if(ioctl(sockClient, FIONREAD, &size) < 0) break;

        if(size >= 8)
        {
          if(recv(sockClient, (char *)&command, 8, MSG_WAITALL) < 0) break;
          switch(command >> 60)
          {
            case 0:
            	if(verbose)printf("Sending phase word %d\n",(int)command);
                /* set phase increment */
                *((uint32_t *)(cfg + FREQ_OFFSET)) = (uint32_t)command;
              	break;

            case 1:

            	samples = command & 0xFFFFFFFF;
            	// reset writer and packetizer
            	  *((uint32_t *)(cfg + 0)) &= 15;//  ( WRITER_ENABLE_FLAG | PKTZR_RESET_FLAG ) ;

         		  // reset trigger
         		  *((uint32_t *)(cfg + 0)) &= ~ TRIGGER_RECORD_FLAG;
            	  // set number of samples
            	  *((uint32_t *)(cfg + RECORD_LENGTH_OFFSET)) = samples;
            	  if(verbose)printf("Entering normal mode. Samples = %d, buffer length = %d bytes\n", samples, length);

            	  // enter normal mode
            	 *((uint32_t *)(cfg + 0)) |= 15; // ( WRITER_ENABLE_FLAG | PKTZR_RESET_FLAG );
            	 // *((uint32_t *)(cfg + 0)) |= PKTZR_RESET_FLAG;
            	//  *((uint32_t *)(cfg + 0)) |=

            	  wait=rand()/1000;
            	  if(verbose)printf("Waiting to trigger for %ld usec\n", wait);
                  usleep(wait);
                  //trigger
                  *((uint32_t *)(cfg + 0)) |= TRIGGER_RECORD_FLAG;
                  sleep(1);
                  if(verbose)printf("Sending data\n");
           		 //read start pos
           		trigger_pos=*((uint32_t *)(sts + RECORD_START_POS_OFFSET));
           		 start_offset=((trigger_pos*2)/packet_size)*packet_size-packet_size;
           		 end_offset=((trigger_pos*2)/packet_size)*packet_size+samples*4-packet_size;
           		 for(offset=0;offset < samples*4;offset +=packet_size)
           		 	 {
           			 	 if(send(sockClient, ram + offset, packet_size, 0) < 0){   perror("send");break;}
           			 }
           		config=*((uint32_t *)(cfg + 0));
           		if(verbose)printf("Start pos = %d, Last offset = %d, Start off = %d, end off = %d, writer sts = %d, offset = %d, config = %x\n",trigger_pos,offset, start_offset, end_offset,*((uint32_t *)(sts + WRITER_STS_OFFSET)),offset,config);
         		  break;
            case 2:
            	interrupted=1;
            	break;
            case 3: //get status
            	offset = command & 0xFFFFFFFF;
            	 status=*((uint32_t *)(sts + offset));
            	 if(verbose>1)printf("STS Offset =%u, Status = %u\n",(uint32_t)offset, status);
   			 	 if(send(sockClient, sts + offset, sizeof(status), 0) < 0){   perror("send");break;}
            	 break;
            case 4: //get temperature
            	  if((fp = fopen("/sys/bus/iio/devices/iio\:device0/in_temp0_raw", "r")) == NULL){
            	              	  	    perror("device open");
            	              	  	    return 1;
            	              	    }
            	  fscanf(fp,"%d", &temperature_raw);
            	  fclose(fp);
            	  temperature=temperature_scale/1000*(temperature_raw+temperature_offset);
            	  // printf("Temperature scale = %lf, offset = %d, raw = %d\nTemperature = %lf\n", temperature_scale, temperature_offset, temperature_raw, temperature);
            	  if(send(sockClient, &temperature, sizeof(temperature), 0) < 0){   perror("send");break;}
            	  break;
            case 5: //set decimation rate
                  value = command & 0xFFFF;
            	  *((uint32_t *)(cfg + 0)) &= ~ UPDATE_CIC_FLAG;
                  *((uint32_t *)(cfg + DESIMATION_OFFSET)) = value;
                  *((uint32_t *)(cfg + 0)) |= UPDATE_CIC_FLAG;
                  value=*((uint32_t *)(cfg + DESIMATION_OFFSET));
              	  config=*((uint32_t *)(cfg + 0));
              	if(verbose)printf("Decimation rate =%u, config = %x\n",(uint32_t)value, config);
                  break;
            case 6:  // arm

             	samples = command & 0xFFFFFFFF;
             	// reset writer and packetizer
             	  *((uint32_t *)(cfg + 0)) &= ~ ( WRITER_ENABLE_FLAG | PKTZR_RESET_FLAG ) ;

          		  // reset trigger
          		  *((uint32_t *)(cfg + 0)) &= ~ TRIGGER_RECORD_FLAG;
             	  // set number of samples
             	  *((uint32_t *)(cfg + RECORD_LENGTH_OFFSET)) = samples;
             	 if(verbose)printf("Entering normal mode. Samples = %d, buffer length = %d bytes\n", samples, length);

             	  // enter normal mode
             	 *((uint32_t *)(cfg + 0)) |= ( WRITER_ENABLE_FLAG | PKTZR_RESET_FLAG );
             	 // *((uint32_t *)(cfg + 0)) |= PKTZR_RESET_FLAG;
             	//  *((uint32_t *)(cfg + 0)) |=

             	if(verbose)printf("Armed, status %d\n", *((uint32_t *)(cfg + 0)));
             	   break;
            case 7: //software trigger
                //trigger
                 *((uint32_t *)(cfg + 0)) |= TRIGGER_RECORD_FLAG;
                 if(verbose)printf("Software trigger, status %d\n",*((uint32_t *)(cfg + 0)));
            	break;
            case 8: //read from start of buffer
             	samples = command & 0xFFFFFFFF;
             	if(verbose)printf("Sending data\n");
          		 //read start pos
          		trigger_pos=*((uint32_t *)(sts + RECORD_START_POS_OFFSET));
          		 start_offset=((trigger_pos*2)/packet_size)*packet_size-packet_size;
          		 end_offset=((trigger_pos*2)/packet_size)*packet_size+samples*4-packet_size;
          		 for(offset=0;offset < samples*4;offset +=packet_size)
          		 	 {
          			 	 if(send(sockClient, ram + offset, packet_size, 0) < 0){   perror("send");break;}
          		 	 }
          		if(verbose)printf("Offset %d\n",offset);
          		config=*((uint32_t *)(cfg + 0));
          		if(verbose)printf("Start pos = %d, Last offset = %d, Start off = %d, end off = %d, writer sts = %d, config = %x\n",trigger_pos,offset, start_offset, end_offset,*((uint32_t *)(sts + WRITER_STS_OFFSET)),config);
                break;
            case 9: //read data chunk
            	start_offset= command & 0x3FFFFFFF;
            	end_offset = (command  >> 30)& 0x3FFFFFFF;
         		trigger_pos=*((uint32_t *)(sts + RECORD_START_POS_OFFSET));
         		if(verbose)printf("Sending data, start = %d, end = %d, trigger pos = %d\n", start_offset,end_offset,trigger_pos);
          		 for(offset=start_offset;offset < end_offset;offset +=packet_size)
          		 	 {
          			 	 if(send(sockClient, ram + offset, packet_size, 0) < 0){   perror("send");break;}
          		 	 }
          		if(verbose)printf("Offset %d\n",offset);
          		config=*((uint32_t *)(cfg + 0));
          		if(verbose)printf("writer sts = %d, config = %x\n",*((uint32_t *)(sts + WRITER_STS_OFFSET)),config);
                break;
            case 10:
            	i = command & 0xFFFF;
            	if(i < SYSTEM_CALL_MAX) {
            		if(verbose)printf("Call: %s\n",system_call[i]);
            		system(system_call[i]);
            	}
            	break;
            case 11: //get config
            	offset = command & 0xFFFFFFFF;
            	 status=*((uint32_t *)(cfg + offset));
            	 if(verbose)printf("CFG Offset =%u, Status = %u\n",(uint32_t)offset, status);
   			 	 if(send(sockClient, cfg + offset, sizeof(status), 0) < 0){   perror("send");break;}
            	 break;
            case 12: //set config
            	offset = (command >> 32)& 0xFF;
            	 status=command & 0xFFFFFFFF;
            	 if(verbose)printf("Set CFG Offset =%u, State = %u\n",(uint32_t)offset, status);
            	 *((uint32_t *)(cfg + offset)) = status;
   			 	 break;
            case 13: // read RX FIFO
            	if(verbose)printf("Read FIFO. Counter =%u\n",*((uint32_t *)(sts + RX_FIFO_CNT_OFFSET)));
            	//while(*((uint32_t *)(sts + RX_FIFO_CNT_OFFSET))< 4096)usleep(500);
                memcpy(buffer, rx_data, 12228);
            	if(verbose)printf("After read FIFO. Counter =%u\n",*((uint32_t *)(sts + RX_FIFO_CNT_OFFSET)));
                if(send(sockClient, buffer, 12228, MSG_NOSIGNAL) < 0){   perror("send FIFO");break;}
                break;
          }
        }
      }
close(sockClient);
if(verbose)printf("closed connection\n");



  munmap(cfg, sysconf(_SC_PAGESIZE));
  munmap(ram, sysconf(_SC_PAGESIZE));
  munmap(sts, sysconf(_SC_PAGESIZE));
  munmap(rx_data, sysconf(_SC_PAGESIZE));
  return 0;
}
