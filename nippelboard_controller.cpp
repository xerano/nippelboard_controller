#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>
#include <unistd.h>
#include <RF24/RF24.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <mpd/connection.h>
#include <mpd/client.h>

#define PIPES 3

#define BTN_MASK_0 (1 << 0)
#define BTN_MASK_1 (1 << 1)
#define BTN_MASK_2 (1 << 2)
#define BTN_MASK_3 (1 << 3)
#define BTN_MASK_4 (1 << 4)
#define BTN_MASK_5 (1 << 5)
#define BTN_MASK_6 (1 << 6)


using namespace std;

struct ButtonState {
  uint8_t pressed;
  uint8_t volume;
} buttonState;

RF24 radio(22, 0);

void eventLoop();
uint8_t button2song(struct ButtonState bs);

const uint8_t pipes[][13] = {"NippelBoard0", "NippelBoard1"};

long map(long x, long in_min, long in_max, long out_min, long out_max){
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void printErrorAndExit(struct mpd_connection *conn)
{
	assert(mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS);

	const char *message = mpd_connection_get_error_message(conn);
	
	fprintf(stderr, "MPD error: %s\n", message);
	mpd_connection_free(conn);
	exit(EXIT_FAILURE);
}

void my_finishCommand(struct mpd_connection *conn)
{
	if (!mpd_response_finish(conn))
		printErrorAndExit(conn);
}



void setup_radio(){
    // Setup and configure rf radio
    radio.begin();
    radio.setRetries(15,15);
    radio.setDataRate(RF24_250KBPS);
    radio.printDetails();
    radio.openReadingPipe(1,pipes[1]);
    radio.openReadingPipe(2,pipes[2]);
    radio.startListening();
}

struct mpd_connection *mpd_connect() {
	struct mpd_connection *conn = mpd_connection_new(NULL, 0, 0);

	if(conn == NULL){
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	
	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
		printErrorAndExit(conn);

	return conn;
}

struct mpd_status *mpd_status;
struct mpd_connection *mpd_conn;

void sighandler(int signum){
	bcm2835_close();
	if(mpd_status != NULL){
		mpd_status_free(mpd_status);	
	}

	if(mpd_conn != NULL){
		mpd_connection_free(mpd_conn);
	}
    exit(0);
}

int main(int argc, char** argv){
	
    if(SIG_ERR == signal(SIGTERM, sighandler)){
        printf("cannot catch SIGTERM");
    }

    if(SIG_ERR == signal(SIGINT, sighandler)){
        printf("cannot catch SIGINT");
    }

    fprintf(stderr, "Setup radio...\n");
    setup_radio();
    eventLoop();
}

void eventLoop(){
	enum mpd_state state;

    fprintf(stderr, "Entering main loop...\n");
	while (1)
	{
		// if there is data ready
		uint8_t pipe = 1;

		if ( radio.available(&pipe) )
		{
			while(radio.available()){
				radio.read(&buttonState, sizeof(buttonState));
			}

			fprintf(stderr, "received(%d) %d %d\n", pipe, buttonState.pressed, buttonState.volume);

			uint8_t songId = button2song(buttonState);
			uint8_t volume = buttonState.volume;

			
			
			fprintf(stderr, "playing song %d\n", songId);		

			mpd_conn = mpd_connect();
			if(mpd_conn != NULL){
				mpd_status = mpd_run_status(mpd_conn);
				if(mpd_status == NULL){
					fprintf(stderr, "Failed to get status from MPD\n");
					fprintf(stderr, "MPD mpd_run_status: %s\n", mpd_connection_get_error_message(mpd_conn));
				} else {

					state = mpd_status_get_state(mpd_status);
					mpd_status_free(mpd_status);
					my_finishCommand(mpd_conn);

					long v = map(volume, 0, 255, 0, 100);
					fprintf(stderr, "setting volume to %ld\n", v);
					mpd_run_set_volume(mpd_conn, v);
					my_finishCommand(mpd_conn);

					mpd_run_single(mpd_conn, true);
					my_finishCommand(mpd_conn);
				
					if(songId != 255){
						if(state == MPD_STATE_STOP){
							fprintf(stderr, "start playing song %d\n", songId);
							mpd_send_play_pos(mpd_conn, songId);
							my_finishCommand(mpd_conn);
						}

						if(state == MPD_STATE_PLAY){
							fprintf(stderr, "pause playing...\n");
							mpd_send_stop(mpd_conn);
							my_finishCommand(mpd_conn);
							mpd_send_play_pos(mpd_conn, songId);
							my_finishCommand(mpd_conn);
						}
					}

				}

				mpd_connection_free(mpd_conn);
			}

			if(pipe>PIPES){
				pipe=1;
			}
		}
		delay(10); //Delay after payload responded to, minimize RPi CPU time
	} // forever loop
}

uint8_t button2song(struct ButtonState bs){
	for(uint8_t i = 0; i < 7; i++) {
		if(bs.pressed & (1 << i)){
			return i;
		}
	}
	return 255;
}
