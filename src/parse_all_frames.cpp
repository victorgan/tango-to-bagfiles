// Copyright 2013 Motorola Mobility LLC. Part of the Trailmix project.
// CONFIDENTIAL. AUTHORIZED USE ONLY. DO NOT REDISTRIBUTE.

#include <superframe_parser/super_frame_parser.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
namespace fs = boost::filesystem;
namespace po = boost::program_options;


int main (int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "Too few arguments.\n";
        std::cout << "Usage: " << argv[0] << " base_dir output.bag\n";
        exit (0);
    }

    fs::path fs_path (argv[1]);
    if (!fs::is_directory (fs_path) || !fs::exists (fs_path))
    {
        std::cout << "Provided first argument (base_dir) is not a valid dir or does not exist!\n";
        return -1;
    }

    po::options_description desc ("Allowed options");
    std::string name_space;
    std::string fisheye_name;
    std::string narrow_name;
    std::string depth_name;
    std::string timestamp_name;
    bool no_narrow = false;
    desc.add_options ()
            ("namespace", po::value<std::string> (&name_space)->default_value ("tango"), "namespace for topics and frame ids")
            ("fisheye", po::value<std::string> (&fisheye_name)->default_value ("fisheye"), "name for fisheye topic and frame id")
            ("narrow", po::value<std::string> (&narrow_name)->default_value ("narrow"), "name for narrow topic and frame id")
            ("pointcloud", po::value<std::string> (&depth_name)->default_value ("depth"), "name for pointcloud topic and frame id")
            ("timestamp", po::value<std::string> (&timestamp_name)->default_value ("images.txt"), "name for the timestamp file")
            ("no_narrow", po::bool_switch (&no_narrow), "provide it if narrow images should not be saved into bag files") ;

    // Parse the command line catching and displaying any
    // parser errors
    po::variables_map vm;
    try
    {
        po::store (po::parse_command_line (argc, argv, desc), vm);
        po::notify (vm);
    }
    catch (std::exception &e)
    {
        std::cout << std::endl << e.what() << std::endl;
        std::cout << desc << std::endl;
    }

    std::cout << no_narrow << std::endl;

    std::stringstream small_img_header;
    small_img_header<< "P5\n" << SMALL_IMG_WIDTH << " " << SMALL_IMG_HEIGHT << "\n255\n";
    std::stringstream big_img_header;
    big_img_header << "P5\n" << BIG_RGB_WIDTH << " " << BIG_RGB_HEIGHT << "\n255\n";
    std::stringstream depth_img_header;
    depth_img_header << "P5\n" << DEPTH_IMG_WIDTH << " " << DEPTH_IMG_HEIGHT << "\n65535\n";

    double prev_depth_timestamp = 0.;
    double prev_narrow_timestamp = 0.;

    // first read all file names
    std::vector<std::string> files;
    fs::directory_iterator it (std::string (argv[1]) + std::string ("/superframes")), eod;
    while (it != eod)
    {
        if (!fs::is_regular_file (it->path ()))
        {
            std::cout << it->path ().string () << "is not a regular file! Continue..." << std::endl;
            continue;
        }
        files.push_back (it->path ().string ());
        it++;
    }
    // sort the file names
    std::sort (files.begin (), files.end ());

    rosbag::Bag bag (argv[2], rosbag::bagmode::Write);
    FILE *fp3;
    // parse the files
    for (size_t i = 0; i < files.size (); i++)
    {
        bool correct_file = true;
        std::string folder_name = std::string (argv[1]) + "/";
        std::cout << "Parsing " << files[i] << "..." << std::endl;
        SuperFrameParser sf_parser (name_space, folder_name + fisheye_name,
                                    folder_name + narrow_name, folder_name + depth_name,
                                    folder_name + timestamp_name);
        try
        {
            sf_parser.parse (files[i]);
        }
        catch (std::exception &e)
        {
            std::cout << "Exception occured: " << e.what () << std::endl << "Continue..." << std::endl;
            correct_file = false;
        }

        if (correct_file)
        {
            // always write small image
            bag.write ("/" + name_space + "/" + fisheye_name + "/image_raw", sf_parser.getFisheyeImage ()->header.stamp, *sf_parser.getFisheyeImage ());
            bag.write ("/" + name_space + "/" + fisheye_name + "/camera_info", sf_parser.getFisheyeCameraInfo ()->header.stamp,
                       *sf_parser.getFisheyeCameraInfo ());

            if (!no_narrow)
            {
                // check if there is a new timestamp for the big image
                // only write to bag, if there is a new capture
                double narrow_timestamp = sf_parser.convertTicksToSeconds (sf_parser.getSuperFrame ()->header.frame.sf_version,
                                                                           sf_parser.getSuperFrame ()->header.frame.big.timestamp);
                if (narrow_timestamp != prev_narrow_timestamp)
                {
                    bag.write ("/" + name_space + "/" + narrow_name + "/image_raw", sf_parser.getNarrowImage ()->header.stamp, *sf_parser.getNarrowImage ());
                    bag.write ("/" + name_space + "/" + narrow_name + "/camera_info", sf_parser.getNarrowCameraInfo ()->header.stamp,
                               *sf_parser.getNarrowCameraInfo ());
                }

                prev_narrow_timestamp = narrow_timestamp;
            }

            // check if there is a new timestamp for the depth image
            // only write to bag, if there is a new capture
            double depth_timestamp = sf_parser.convertTicksToSeconds (sf_parser.getSuperFrame ()->header.frame.sf_version,
                                                                      sf_parser.getSuperFrame ()->header.frame.depth.timestamp);
            if (depth_timestamp != prev_depth_timestamp)
            {
                bag.write ("/" + name_space + "/" + depth_name + "/pointcloud", sf_parser.getPointCloud ()->header.stamp, *sf_parser.getPointCloud ());
                bag.write ("/" + name_space + "/" + depth_name + "/image_raw", sf_parser.getDepthImage ()->header.stamp, *sf_parser.getDepthImage ());
                bag.write ("/" + name_space + "/" + depth_name + "/camera_info", sf_parser.getDepthCameraInfo() ->header.stamp, *sf_parser.getDepthCameraInfo ());

                // save depth img to disk
//                std::stringstream ss_depth;
//                ss_depth << "depth_img_" << fs::path (files[i]).filename ().string() ;
//                if ((fp3 = fopen (ss_depth.str ().c_str (), "wb")) != NULL)
//                {
//                    fprintf (fp3, depth_img_header.str ().c_str ());
//                    fwrite (&sf_parser.getDepthImage()->data[0], 2, DEPTH_IMG_WIDTH * DEPTH_IMG_HEIGHT, fp3);
//                    fclose (fp3);
//                }
            }

            prev_depth_timestamp = depth_timestamp;
        }
    }

    bag.close();
    return 0;
}
