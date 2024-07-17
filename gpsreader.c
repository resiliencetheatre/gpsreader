/* 
 *  gpsreader - simple gpsd client
 *  Copyright (C) 2024  Resilience Theatre
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 *  Writes location string to /tmp/gpssocket (fifo file) 
 * 
 *  [mode],[mode_id],[timestamp],[lat],[lon],[speed],[track],[sat_used],[sat_visible]
 * 
 */
         
#include <gps.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define MODE_STR_NUM 5
static char *mode_str[MODE_STR_NUM] = {
    "n/a",
    "None",
    "2D",
    "3D",
    "Manual"
};

void remove_last_char(char *str) {
    size_t length = strlen(str);
    if (length > 0) {
        str[length - 1] = '\0';
    }
}

int main(int argc, char *argv[])
{
    char timestamp[30];
    char outbuf[100];
    char manual_location_buffer[256];
    struct gps_data_t gps_data;
    FILE *fifowrite;
    FILE *livefifowrite;
    FILE *manual_location_file;
    
    time_t raw_time;
    struct tm *time_info;
    
    memset( timestamp, 0, 30 );
    memset( outbuf, 0, 100 );
    memset( manual_location_buffer, 0, 256 ); 
    if (0 != gps_open("localhost", "2947", &gps_data)) {
        printf("Cannot connect to port 2947.\n");
        return 1;
    }
    printf("gpsreader v0.42 \n");
    (void)gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);

        while (gps_waiting(&gps_data, 5000000)) {
            // /tmp/gpssocket provides location to map UI
            fifowrite = fopen("/tmp/gpssocket", "w");
            // /tmp/livegps provides location to meshpipe.py
            livefifowrite = fopen("/tmp/livegps", "w");
            if ( fifowrite == NULL ) 
            {
                printf("[ERROR] Cannot open /tmp/gpssocket for writing.\n");
            }
            if ( livefifowrite == NULL ) 
            {
                printf("[ERROR] Cannot open /tmp/livegps for writing.\n");
            }
            // Get location from file if file is present
            manual_location_file = fopen("/opt/edgemap-persist/location.txt", "r");
            if (manual_location_file != NULL) {
                // Get the current time & Convert it to local time representation
                time(&raw_time);
                time_info = localtime(&raw_time);
                memset( timestamp, 0, 30 );
                strftime(timestamp, 20, "%Y-%m-%d,%H:%M:%S", time_info );
                // Read location file 
                if ( fgets(manual_location_buffer, sizeof(manual_location_buffer), manual_location_file) != NULL )
                {
                    remove_last_char(manual_location_buffer);
                }
                fclose(manual_location_file);                
                sprintf(outbuf,"Manual,4,%s,%s,0.0,233,19,0\n",timestamp,manual_location_buffer);
                sleep(1);
            }
            else 
            {
                // GPS read if manual location file is not present
                if ( gps_read(&gps_data, NULL, 0) == -1 ) {
                    printf("Read error.\n");
                    break;
                }
                if ( (MODE_SET & gps_data.set) != MODE_SET ) {
                    continue;
                }
                if ( gps_data.fix.mode < 0 || gps_data.fix.mode >= MODE_STR_NUM ) {
                    gps_data.fix.mode = 0;
                }
                if ( (TIME_SET & gps_data.set) == TIME_SET ) {
                    memset( timestamp, 0, 30 );
                    strftime(timestamp, 20, "%Y-%m-%d,%H:%M:%S", localtime(&gps_data.fix.time.tv_sec));
                } 
                if (isfinite(gps_data.fix.latitude) && isfinite(gps_data.fix.longitude)) {
                    sprintf(outbuf,"%s,%d,%s,%.8f,%.8f,%0.1f,%1.0f,%d,%d \n",mode_str[gps_data.fix.mode], gps_data.fix.mode,timestamp,gps_data.fix.latitude, gps_data.fix.longitude,gps_data.fix.speed,gps_data.fix.track,gps_data.satellites_used,gps_data.satellites_visible);
                } else {
                    sprintf(outbuf,"%s,%d,-,-,-,-,-,-,- \n",mode_str[gps_data.fix.mode], gps_data.fix.mode);
                }
            }
            fprintf(fifowrite,outbuf);
            fprintf(livefifowrite,outbuf);
            fclose(fifowrite);
            fclose(livefifowrite);
            memset( outbuf, 0, 100 );
        }

    (void)gps_stream(&gps_data, WATCH_DISABLE, NULL);
    (void)gps_close(&gps_data);
    return 0;
}
