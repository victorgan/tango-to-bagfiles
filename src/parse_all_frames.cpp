// Copyright 2013 Motorola Mobility LLC. Part of the Trailmix project.
// CONFIDENTIAL. AUTHORIZED USE ONLY. DO NOT REDISTRIBUTE.
//
// To compile as a standalone app, 'gcc main.cc' and replace the
// header superframe_v2.h below from github or the SDK website docs.

// Keep this in, it is required for standalone compilation.

#ifndef NOT_COMPILING_WITH_MDK
#define NOT_COMPILING_WITH_MDK
#endif
// Tick Rate for Peanut, SF version 0x100
#define PEANUT_TICKS_PER_MICROSECOND 180.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <superframe_parser/superframe_v2.h>

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include <rosbag/bag.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
namespace fs = boost::filesystem3;

#define PRINTE(indent, x, y, z) fprintf(stdout, indent #z " " y "\n", x.z)
#define PRINT_IMG_HDR(x) \
    PRINTE("\t\t\t", sf->header.frame.x, "%1$u (0x%1$0X)", exposureUs); \
    PRINTE("\t\t\t", sf->header.frame.x, "%1$u (0x%1$0X)", color_temp_k);\
    PRINTE("\t\t\t", sf->header.frame.x, "%1$u (0x%1$0X)", af_setting);\
    PRINTE("\t\t\t", sf->header.frame.x, "%1$u (0x%1$0X)", CAM_GAIN);\
    PRINTE("\t\t\t", sf->header.frame.x, "%1$u (0x%1$0X)", CAM_EXP);\
    PRINTE("\t\t\t", sf->header.frame.x, "%1$u (0x%1$0X)", tear_flag);\

void yuv420p_to_sf2(sf2_t *sf, uint16_t *buffer, const size_t size) {
    memcpy(sf, buffer, size/*sizeof(*sf)*/);
}

double ConvertTicksToSeconds(uint32_t superframe_version,
                             const TimeStamp& raw_timestamp) {
    if (superframe_version & 0x100) {
        return (((static_cast<uint64_t>(raw_timestamp.superframe_v2.ticks_hi) << 32)
                 | (static_cast<uint64_t>(raw_timestamp.superframe_v2.ticks_lo))) /
                (1000000. * PEANUT_TICKS_PER_MICROSECOND));
    } else {
        return static_cast<double>(raw_timestamp.superframe_v1) /
                (static_cast<double>(PEANUT_TICKS_PER_MICROSECOND) * 1000. * 1000.);
    }
}


bool parseFile (std::string file_name, FILE *fp, uint16_t *buffer, sf2_t *sf, int cols, int rows, float bpp)
{
    int bytes_read;

    // Parse the PGM file, skipping the # comment bits.  The # comment pad is
            // to maintain a 4kB block alignment for EXT4 writing.
    unsigned int img_width, img_height;
    char comment_str[8192] = {0};

    if (fscanf (fp, "P5\n%d %d\n", &img_width, &img_height) != 2) {
        fprintf(stderr, "Failed to parse header dimensions\n");
        return false;
    }
    printf("PGM is %dx%d\n", img_width, img_height);
    char *ret_str = fgets(comment_str, 4082, fp);
    if (ret_str == NULL) {
        fprintf(stderr, "Failed to parse comments!!!\n\n\n");
        return false;
    } else {
        printf("Parsed %d comment characters\n",
               static_cast<int>(strlen(comment_str)));
    }
    unsigned int max_val;
    if (fscanf(fp, "%d\n", &max_val) != 1) {
        fprintf(stderr, "Failed to parse max value\n");
        return false;
    }
    printf("PGM Maxval is %d\n", max_val);

    // Read in the data portion starting here.
    bytes_read = fread(buffer, 1, cols*rows*bpp, fp);
    fprintf(stdout, "Read %d bytes\n", bytes_read);

    yuv420p_to_sf2(sf, buffer, cols*rows*bpp);

    // Print out the meta-data.
    fprintf(stdout, "Frame Data\n");
    printf("FileName: %s, Frame count: %d, New Flags: %x, Valid Flags: %x\n",
           file_name.c_str (),
           sf->header.frame.super_frame_count,
           sf->header.frame.sf_valid_flags[1],
           sf->header.frame.sf_valid_flags[0]);
    printf("Superframe Version: 0x%x\nFirmware Version: %u %05u\n",
           sf->header.frame.sf_version,
           sf->header.frame.firmware_version[1],
           sf->header.frame.firmware_version[0]);
    printf("IMU Packet Count: %d\n",
           sf->header.frame.imu_packet_count);

    // This is ONLY tested on SF version 0x100.
    if (sf->header.frame.sf_version != 0x100) {
        fprintf(stderr, "Unknown SF Version 0x%x.  Expected 0x100. Stopped.",
                sf->header.frame.sf_version);
        return false;
    }

    fprintf(stdout, "Small Img (Wide Angle)\n");
    printf("\t\t\tTimestamp (s): %lf\n",
           ConvertTicksToSeconds(sf->header.frame.sf_version,
                                 sf->header.frame.small.timestamp));
    PRINT_IMG_HDR(small);
    fprintf(stdout, "Big Img (4MP Sensor)\n");
    printf("\t\t\tTimestamp (s): %lf\n",
           ConvertTicksToSeconds(sf->header.frame.sf_version,
                                 sf->header.frame.big.timestamp));
    PRINT_IMG_HDR(big);
    fprintf(stdout, "Depth Img\n");
    printf("\t\t\tTimestamp (s): %lf\n",
           ConvertTicksToSeconds(sf->header.frame.sf_version,
                                 sf->header.frame.depth.timestamp));
    PRINT_IMG_HDR(depth);

#if 0
    // Performance counters are disabled in firmware by default.
    const bool kProfileData = false;
    if (kProfileData) {
        fprintf(stdout, "Profile Data\n");
        PRINTE("\t", sf->header.profile, "%1$u (0x%1$0X)", pixel_pipe_time);
        PRINTE("\t", sf->header.profile, "%1$u (0x%1$0X)", optical_flow_time);
        PRINTE("\t", sf->header.profile, "%1$u (0x%1$0X)", feature_maint_time);
        PRINTE("\t", sf->header.profile, "%1$u (0x%1$0X)", feature_count);

        fprintf(stdout, "\t\tBad Feature Removal Stage\n");
        PRINTE("\t\t\t", sf->header.profile.fm1, "%1$u (0x%1$0X)", id);
        PRINTE("\t\t\t", sf->header.profile.fm1, "%1$u (0x%1$0X)", time);
        PRINTE("\t\t\t", sf->header.profile.fm1, "%1$u (0x%1$0X)", feature_count);

        fprintf(stdout, "\t\tNew Feature Search Stage\n");
        PRINTE("\t\t\t", sf->header.profile.fm2, "%1$u (0x%1$0X)", id);
        PRINTE("\t\t\t", sf->header.profile.fm2, "%1$u (0x%1$0X)", time);
        PRINTE("\t\t\t", sf->header.profile.fm2, "%1$u (0x%1$0X)", feature_count);

        fprintf(stdout, "\t\tNew Feature Admitted Stage\n");
        PRINTE("\t\t\t", sf->header.profile.fm3, "%1$u (0x%1$0X)", id);
        PRINTE("\t\t\t", sf->header.profile.fm3, "%1$u (0x%1$0X)", time);
        PRINTE("\t\t\t", sf->header.profile.fm3, "%1$u (0x%1$0X)", feature_count);
    }

    // Print IMU data.  Note, to convert to physical units, refer to the github.
    // This is only to show timestamps and check if data is present.
    fprintf(stdout, "IMU Data, Raw Only\n");
    fprintf(stdout, "Contact project-tango-devrel@google.com ");
    fprintf(stdout, "for conversion/scale ranges.\n");
    for (unsigned int i = 0;
         i < sf->header.frame.imu_packet_count && i < MAX_IMU_COUNT; ++i) {
        switch (sf->header.imu[i].type) {
        case sf_imu_data_t::IMU_ACCEL:
            fprintf(stdout, "\tACCEL\n");
            printf("\tTimestamp (s): %lf\n",
                   ConvertTicksToSeconds(sf->header.frame.sf_version,
                                         sf->header.imu[i].timestamp));

            fprintf(stdout, "\t%d %d %d %d\n",
                    sf->header.imu[i].data.accel[0], sf->header.imu[i].data.accel[1],
                    sf->header.imu[i].data.accel[2], sf->header.imu[i].data.accel[3]);
            break;
        case sf_imu_data_t::IMU_GYRO:
            fprintf(stdout, "\tGYRO\n");
            printf("\tTimestamp (s): %lf\n",
                   ConvertTicksToSeconds(sf->header.frame.sf_version,
                                         sf->header.imu[i].timestamp));
            fprintf(stdout, "\t%d %d %d %d\n",
                    sf->header.imu[i].data.gyro[0], sf->header.imu[i].data.gyro[1],
                    sf->header.imu[i].data.gyro[2], sf->header.imu[i].data.gyro[3]);
            break;
        case sf_imu_data_t::IMU_MAG:
            fprintf(stdout, "\tMAG\n");
            printf("\tTimestamp (s): %lf\n",
                   ConvertTicksToSeconds(sf->header.frame.sf_version,
                                         sf->header.imu[i].timestamp));
            fprintf(stdout, "\t%d %d %d %d\n",
                    sf->header.imu[i].data.mag[0], sf->header.imu[i].data.mag[1],
                    sf->header.imu[i].data.mag[2], sf->header.imu[i].data.mag[3]);
            break;
        case sf_imu_data_t::IMU_BARO:
            fprintf(stdout, "\tBARO\n");
            printf("\tTimestamp (s): %lf\n",
                   ConvertTicksToSeconds(sf->header.frame.sf_version,
                                         sf->header.imu[i].timestamp));
            fprintf(stdout, "\t%d\n", sf->header.imu[i].data.barometer);
            break;
        case sf_imu_data_t::IMU_TEMP:
            fprintf(stdout, "\tTEMP\n");
            printf("\tTimestamp (s): %lf\n",
                   ConvertTicksToSeconds(sf->header.frame.sf_version,
                                         sf->header.imu[i].timestamp));
            fprintf(stdout, "\t%d\n", sf->header.imu[i].data.temperature);
            break;
        case sf_imu_data_t::IMU_INVALID:
        default:
            fprintf(stdout, "\tINVALID_IMU\n");
            break;
        }
    }

    // Print out feature tracking data.
    fprintf(stdout, "VTRACK\n");
    PRINTE("\t", sf->header.vtrack, "%1$u (0x%1$0X)", max_features);
    PRINTE("\t", sf->header.vtrack, "%1$u (0x%1$0X)", prev_feature_count);
    PRINTE("\t", sf->header.vtrack, "%1$u (0x%1$0X)", curr_feature_count);

    fprintf(stdout, "\tprevious\n");
    for (unsigned int i = 0 ; i < MAX_FEATURE_CELL_COUNT ; ++i) {
        for (unsigned int j = 0 ;
             j < sf->header.vtrack.features_per_cell[i] ; ++j) {
            int k = i*MAX_FEATURES_PER_CELL + j;
            PRINTE("\t\t", sf->header.vtrack.prev_meta[k], "%1$u (0x%1$0X)", id);
            PRINTE("\t\t", sf->header.vtrack.prev_meta[k], "%1$u (0x%1$0X)", age);
            PRINTE("\t\t", sf->header.vtrack.prev_meta[k], "%f", harris_score);
            fprintf(stdout, "\t\tx, y %f, %f\n",
                    sf->header.vtrack.prev_xy[k].x, sf->header.vtrack.prev_xy[k].y);
        }
    }

    fprintf(stdout, "\tcurrent\n");
    for (unsigned int i = 0 ; i < MAX_FEATURE_CELL_COUNT ; ++i) {
        for (unsigned int j = 0 ; j < sf->header.vtrack.features_per_cell[i]
             && j < MAX_FEATURE_CELL_COUNT ; ++j) {
            int k = i*MAX_FEATURES_PER_CELL + j;
            PRINTE("\t\t", sf->header.vtrack.curr_meta[k], "%1$u (0x%1$0X)", id);
            PRINTE("\t\t", sf->header.vtrack.curr_meta[k], "%1$u (0x%1$0X)", age);
            PRINTE("\t\t", sf->header.vtrack.curr_meta[k], "%f", harris_score);
            fprintf(stdout, "\t\tx, y %f, %f\n",
                    sf->header.vtrack.curr_xy[k].x, sf->header.vtrack.curr_xy[k].y);
        }
    }
#endif
    return true;
}


int main(int argc, char **argv)
{
    int cols, rows;
    float bpp;

    cols = 1280;
    //    cols = 640;
    // Note: "Small" superframes have size 640x896 (=1280x448).
    // The "header" saves space for the Y plane for the BigRGB image,
    // and so it uses 1168 rows.
    // A "Full" superframe should have 1752 rows (additional space
    // on top of 1168 is for the UV plane).
    rows = 1168;
    //    rows = 896;
    bpp = 1.5;
    //    bpp = 1;

    uint16_t *buffer;
    sf2_t *sf;
    FILE *fp;
    if (argc != 2) {
        printf ("Too few arguments.\n");
        printf ("Usage: sf2_parser {filename.pgm}\n");
        exit (0);
    }


    buffer = static_cast<uint16_t*> (malloc (cols * rows * bpp));

    sf = static_cast<sf2_t*> (malloc (sizeof (sf2_t)));

    fs::path fs_path (argv[1]);
    if (!fs::is_directory (fs_path) || !fs::exists (fs_path))
        return -1;


    fs::directory_iterator it (argv[1]), eod;
    rosbag::Bag bag ("test.bag", rosbag::bagmode::Write);
    sensor_msgs::Image bag_small_img;
    bag_small_img.header.frame_id = "superframe/small_image";
    bag_small_img.height = SMALL_IMG_HEIGHT;
    bag_small_img.width = SMALL_IMG_WIDTH;
    bag_small_img.encoding = "mono8";
    bag_small_img.data.resize (SMALL_IMG_WIDTH * SMALL_IMG_HEIGHT);
    sensor_msgs::Image bag_big_img;
    bag_big_img.header.frame_id = "superframe/big_image";
    bag_big_img.height = BIG_RGB_HEIGHT;
    bag_big_img.width = BIG_RGB_WIDTH;
    bag_big_img.encoding = "rgb8";
    bag_big_img.data.resize (BIG_RGB_HEIGHT * BIG_RGB_WIDTH);

    while (it != eod)
    {
        if (fs::is_regular_file (it->path ()))
        {
            fp = fopen (it->path ().c_str (), "rb");
            if (fp == NULL)
            {
                fprintf (stderr, "Failed to open file %s\n", it->path ().c_str ());
                continue;
            }
            if (!parseFile (it->path ().string (), fp, buffer, sf, cols, rows, bpp))
                continue;

            bag_small_img.header.stamp = ros::Time (ConvertTicksToSeconds (sf->header.frame.sf_version, sf->header.frame.small.timestamp));
            memcpy (&bag_small_img.data[0], sf->small_img, (SMALL_IMG_HEIGHT * SMALL_IMG_WIDTH));
            bag.write ("tango/small_image", bag_small_img.header.stamp, bag_small_img);

            bag_big_img.header.stamp = ros::Time (ConvertTicksToSeconds (sf->header.frame.sf_version, sf->header.frame.big.timestamp));
            memcpy (&bag_big_img.data[0], sf->big_rgb, (BIG_RGB_HEIGHT * BIG_RGB_WIDTH));
            bag.write ("tango/big_image", bag_big_img.header.stamp, bag_big_img);

            fclose (fp);
        }
        it++;
    }

    bag.close();
    return 0;
}