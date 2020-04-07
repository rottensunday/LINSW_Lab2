#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define	CONSUMER	    "Consumer"
#define MAXINPUT        10
#define CHIPNAME        "gpiochip1"
#define BUTTONSSTART    12
#define DIODESSTART     24
#define DOTWAIT         10
#define BOUNCEWAIT      2e8

typedef struct user_input{
    int vals[MAXINPUT];
    int length;
} user_input;

const int zero[3] = { 0,0,0 };
const int one[3] = { 0,0,1 };
const int two[3] = { 0,1,0 };
const int three[3] = { 0,1,1 };
const int four[3] = { 1,0,0 };
const int five[3] = { 1,0,1 };
const int six[3] = { 1,1,0 };
const int seven[3] = { 1,1,1 };

void init_input(user_input* input);
void show_result(user_input* input, struct gpiod_line_bulk* diodes_lines);
int quat_to_dec(user_input* input);

int main(int argc, char **argv)
{
	unsigned int button_lines_ids[4] = { BUTTONSSTART, BUTTONSSTART + 1, BUTTONSSTART + 2, BUTTONSSTART + 3 };
    unsigned int diodes_lines_ids[3] = { DIODESSTART, DIODESSTART + 1, DIODESSTART + 2 };
    struct gpiod_line_bulk button_lines;
    struct gpiod_line_bulk diodes_lines;
    struct gpiod_line_bulk fired_lines;
    struct gpiod_line* temp_line;
	struct timespec ts = { DOTWAIT, 0 };
    struct timespec bounce_wait = { 0, BOUNCEWAIT };
	struct gpiod_line_event event;
	struct gpiod_chip *chip;
    user_input input;
	int i, ret, offset, line_offset;

    printf("Starting chip!\n");
	chip = gpiod_chip_open_by_name(CHIPNAME);
	if (!chip) {
		perror("Open chip failed\n");
		ret = -1;
		goto end;
	}

    printf("Getting button lines!\n");
    ret = gpiod_chip_get_lines(chip, button_lines_ids, 4, &button_lines);
    if (ret < 0){
        perror("Get button lines failed!\n");
        ret = -1;
        goto close_chip;
    }
    printf("Successfully got button lines from chip!\n");

    printf("Getting diodes lines!\n");
    ret = gpiod_chip_get_lines(chip, diodes_lines_ids, 3, &diodes_lines);
    if (ret < 0){
        perror("Get diodes lines failed!\n");
        ret = -1;
        goto close_chip;
    }
    printf("Successfully got diodes lines from chip!\n");

    printf("Requesting events from buttons!\n");
	ret = gpiod_line_request_bulk_falling_edge_events(&button_lines, CONSUMER);
	if (ret < 0) {
		perror("Request event notification failed\n");
		ret = -1;
		goto release_line;
	}
    printf("Successfully requested events from buttons!\n");

    printf("Requesting output on diodes!\n");
    ret = gpiod_line_request_bulk_output(&diodes_lines, CONSUMER, zero);
	if (ret < 0) {
		perror("Request output notification failed\n");
		ret = -1;
		goto release_line;
	}
    printf("Successfully requested output on diodes!\n");

    // init input struct: fill everything with -1
    init_input(&input);

    printf("Starting main loop!\n");
    while (true){
		ret = gpiod_line_event_wait_bulk(&button_lines, &ts, &fired_lines);
		if (ret < 0) {
			perror("Wait event notification failed\n");
			ret = -1;
			goto release_line;
		} else if (ret == 0) {
			printf(".\n");
			continue;
		}

        // We successfully waited and now process some new events
        gpiod_line_bulk_foreach_line_off(&fired_lines, temp_line, offset){
            line_offset = gpiod_line_offset(temp_line);
            // corresponding value is value inserted to user_input struct
            if(line_offset != BUTTONSSTART + 3){
                printf("Line fired: %d --- value %d\n", line_offset, line_offset - BUTTONSSTART);
            }
            else{
                printf("Line fired: %d --- time to show!\n", line_offset);
            }
            // we read corresponding event so it doesn't stick for next iteration
            ret = gpiod_line_event_read(temp_line, &event);
            if (ret < 0) {
                perror("Read last event notification failed\n");
                ret = -1;
                goto release_line;
		    }
            // we now should handle bouncing: we wait for 200 ms for new events.
            // if some events shows up, we just read it and wait for another.
            // if 200 ms pass, we assume that we're in stable state and can continue
            // (200 ms is set in bounce_wait)
            while(true){
                ret = gpiod_line_event_wait(temp_line, &bounce_wait);
                if(ret == 0) break;
                if (ret < 0) {
                    perror("Read last event notification failed\n");
                    ret = -1;
                    goto release_line;
		        }
                gpiod_line_event_read(temp_line, &event);
            }
            // we populate input struct or proceed to convert and show number
            switch (line_offset){
                case BUTTONSSTART:
                    input.vals[input.length] = 0;
                    input.length++;
                    break;
                case (BUTTONSSTART + 1):
                    input.vals[input.length] = 1;
                    input.length++;
                    break;
                case (BUTTONSSTART + 2):
                    input.vals[input.length] = 2;
                    input.length++;
                    break;
                case (BUTTONSSTART + 3):
                    show_result(&input, &diodes_lines);
                    init_input(&input);
                    break;
            }
        }
	}

	ret = 0;

release_line:
	gpiod_line_release_bulk(&button_lines);
    gpiod_line_release_bulk(&diodes_lines);
    gpiod_line_release_bulk(&fired_lines);
close_chip:
	gpiod_chip_close(chip);
end:
	return ret;
}

void init_input(user_input* input){
    input->length = 0;
    for(int i = 0; i < MAXINPUT; i++){
        input->vals[i] = -1;
    }
}

int tern_to_dec(user_input* input){
    int retval = 0;
    int multiplier = 1;
    for(int i = 0; i < input->length; i++){
        retval += (input->vals[i] * multiplier);
        multiplier *= 3;
    }
    return retval;
}

user_input dec_to_octal(int val){
    user_input retval;
    init_input(&retval);
    for(int i = 0; val > 0; i++){
        retval.vals[i] = val % 8;
        retval.length++;
        val = val / 8;
    }
    return retval;
}

// biggest problem with showing results:
// no result (waiting or it ended) is the same as 0.
void show_result(user_input* input, struct gpiod_line_bulk* diodes_lines){
    if(input->length <= 0) return;
    // show user input
    for(int i = 0; i < input->length; i++){
        printf("input[%d] = %d\n", i, input->vals[i]);
    }
    // show decimal value of user input
    int dec = tern_to_dec(input);
    printf("Decimal val: %d\n", dec);
    // show octal value of user input
    user_input oct_result = dec_to_octal(dec);
    for(int i = 0; i < oct_result.length; i++){
        printf("oct_result[%d] = %d\n", i, oct_result.vals[i]);
    }
    struct gpiod_line* temp_line;
    for (int i = 0; i < (oct_result.length)*2; i++) {
        if(i % 2 == 1){
            gpiod_line_set_value_bulk(diodes_lines, zero);
            sleep(1);
        }
        else{
            switch (oct_result.vals[i/2]){
                case 0:
                    gpiod_line_set_value_bulk(diodes_lines, zero);
                    break;
                case 1:
                    gpiod_line_set_value_bulk(diodes_lines, one);
                    break;
                case 2:
                    gpiod_line_set_value_bulk(diodes_lines, two);
                    break;
                case 3:
                    gpiod_line_set_value_bulk(diodes_lines, three);
                    break;
                case 4:
                    gpiod_line_set_value_bulk(diodes_lines, four);
                    break;
                case 5:
                    gpiod_line_set_value_bulk(diodes_lines, five);
                    break;
                case 6:
                    gpiod_line_set_value_bulk(diodes_lines, six);
                    break;
                case 7:
                    gpiod_line_set_value_bulk(diodes_lines, seven);
                    break;
            }
            sleep(3);
        }
	}

}