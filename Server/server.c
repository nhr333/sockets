#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

int main( int args, char *argv[] ) {

// create socket
  int servsock = socket( AF_INET, SOCK_STREAM, 0 );
  if(servsock == -1){
    printf("socket could not be created\n");
    return 1;
  }

  printf("socket created\n");

  // create server socket address
  struct sockaddr_in servaddr;
  memset( &servaddr, 0, sizeof(servaddr) );
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(5431);
  servaddr.sin_addr.s_addr = INADDR_ANY;

  // configure servsock using servaddr
  if(bind( servsock, (struct sockaddr *)&servaddr, sizeof(servaddr) ) < 0){
    printf("bind failed\n");
    return 1;
  }
  printf("bind completed\n");

  while(1){
    // make servsock listen for incoming client connections
    listen( servsock, 5 );
    printf("listening....\n");

    // accept client connection and log connection information
    struct sockaddr_in cliaddr;
    int cliLen;
    cliLen = sizeof( struct sockaddr_in );
    int clisock = accept( servsock, (struct sockaddr *)&cliaddr, (socklen_t*) &cliLen );
    if(clisock < 0){
      printf("Accept failed\n");
      return 1;
    }
    printf("connection accepted\n");

    char  buf[6000];
    int   bytesRecv;
    FILE* file;
    char  filename[200];

    while(1) {
      printf("waiting for client...\n");
      bytesRecv = recv( clisock, buf, 6000, 0 );

      // 1 - LIST CONTENTS OF CURRENT DIRECTORY *************************************
      if(buf[0] == '1'){
	char listBuf[6000]; //array to hold the list of the directory
	memset(buf, 0, sizeof(buf)); //resets the buf array
	char *ptr = NULL; // to point to the current working directory
	DIR *dPtr = NULL; // directory pointer to the path of the directory
	struct dirent *strDir = NULL; //struct to read the contents of the directory
	char test[200]; // to hold current working directory
	getcwd(test, 200); //to get the current working directory
	ptr = test; //to point ptr to the current working directory
	dPtr = opendir((const char*) ptr); //to open the directory and point the DIR variable to the directory
	//while loop to read the directory contents
	while((strDir = readdir(dPtr)) != NULL){
	  strcpy(buf, strDir->d_name); //copy contents of directory in buf
	  strcat(buf, "  \t"); //to add a tab to the end of a content in the directory
	  strcat(listBuf, buf); //to concat the newly read content to the previously read content stored in listBuf
	  memset(buf, 0, sizeof(buf)); // to reset buf to ensure no duplication
	}

	strcpy(buf, listBuf);
	memset(listBuf, 0, sizeof(listBuf));
	send(clisock, buf, strlen(buf), 0);
	memset(buf, 0, sizeof(buf)); // to reset the buf
      }

      // 2 - GET A FILE FROM SERVER *************************************************
      else if( buf[0] == '2') {
	memset(buf, 0, sizeof(buf));
	recv( clisock, filename, 6000, 0 );

	file = fopen(filename, "rb"); //open the file name for reading
	printf("file name is %s\n", filename);
	memset(filename, 0, sizeof(filename));

	if(file == NULL){
	  printf("file did not open\n");
	  return 1;
	}

	memset(buf, 0, sizeof(buf));

	while(1){
	  unsigned char sending[256] = {0};
	  int readF = fread(sending, 1, 256, file);

	  if(readF > 0){
	    printf("sending to client\n");
	    send(clisock, sending, readF, 0);
	  }
	  memset(sending, 0, sizeof(sending));

	  if (readF < 256){
	    if (feof(file))
	      printf("File sending complete.....\n");
	    if (ferror(file))
	      printf("Error reading\n");
	    break;
	  }
	}
	fclose(file); //close the file
	printf("closed file!\n");

      }

      // 3 - SEND A FILE TO SERVER ****************************************************
      else if( buf[0] == '3') {
      
	memset(buf, 0, sizeof(buf)); // to reset the buf
	if(recv(clisock, filename, 200, 0) < 0){
	  printf("receive failed\n");
	  exit(1);
	}
	file = fopen(filename, "wb"); // make file with write option
	if (file == NULL) {
	  printf("Could not open file.\n");
	  exit(1);
	}

	int bytesReceived = 0;
	char recvBuff[256];
      
	/* Receive data in chunks of 256 bytes */
	while((bytesReceived = recv(clisock, recvBuff, 256, 0)) > 0){
	  printf("Bytes received %d\n",bytesReceived);    
	  // recvBuff[n] = 0;
	  fwrite(recvBuff, 1,bytesReceived,file);
	  printf("written\n");
	  memset(recvBuff, 0, sizeof(recvBuff));

	  if(bytesReceived < 256){
	    printf("in the nested if\n");
	    break;
	  }
	  // printf("%s \n", recvBuff);
	  //bytesReceived = 0;
	}
	printf("outside of the while!!!\n");

	if(bytesReceived < 0){
	  printf("\n Read Error \n");
	}

	fwrite(buf, 1, sizeof(buf), file);
	fclose(file);
      }

      // 4 - CHANGE DIRECTORY *********************************************************
      else if (buf[0] == '4'){
	memset(buf, 0, sizeof(buf));
	bytesRecv = recv(clisock, buf, 6000, 0); //to receive directory name
	int status; //check to ensure changing the dir was successful
	status = chdir(buf); //change the directory
	if(status == 0){
	  printf("changes dir");
	}
	strcpy(buf, "_DIR_CHANGED_\n");
	send(clisock, buf, strlen(buf), 0);
	memset(buf, 0, sizeof(buf)); // to reset the buf
      }

      // 5 - CREATE DIRECTORY *********************************************************
      else if (buf[0] == '5'){
	memset(buf, 0, sizeof(buf)); //reset of buf
	bytesRecv = recv(clisock, buf, 6000, 0); //to receive directory name
	int status;
	status = mkdir(buf, S_IRWXU | S_IRWXG | S_IRWXO); //make dir with name in buf and give permissions
	memset(buf, 0, sizeof(buf)); //reset buf
	strcpy(buf, "Directory Made"); //to provide feedback to client
	send(clisock, buf, strlen(buf), 0);
	memset(buf, 0, sizeof(buf)); // to reset the buf
      }

      // 6 - exit *********************************************************
      else if (buf[0] == '6'){
	printf("Goodbye\n");
	break;
      } 
      sleep(1);
    }
  }
  // close socket, once finished
  close( servsock );
  printf("socket closed\n");
  return 0;
}
