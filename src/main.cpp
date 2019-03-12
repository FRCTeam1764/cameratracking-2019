/*

int main()
{

	return 0;
}

*/
#include <unistd.h>
#include <vector>
#include <functional>
#include <map>
#include <optional>
#include <sys/time.h>
#include <cmath>


#include "opencv2/opencv.hpp"
#include "opencv2/imgproc.hpp"

#include "cli/flag_interpreter.hpp"
#include "network/network.hpp"
#include "serialization/serialization.hpp"

using namespace cv;


//Non-configurable constants:

const double to_rad = (3.141592654f / 180.f);
const double to_deg = (180.f / 3.141592654f);

//TODO : file with all of these configs
//Configurable constants:

const unsigned int res_x = 640;
const unsigned int res_y = 480;


//used to determine the angle given px from center
const float angle_coeffecient =  ( 120.f * to_rad ) /sqrt(pow(res_x,2)+pow(res_y,2));

const double real_length = 4.875; //inches
//the measured length @ 1 meter
const double calib_measured_length = 31.400637f;// camera px @ 640x480
const double focal_length = (calib_measured_length*39.37)/(real_length);

const double camera_location_inches_from_center = 14.f;
/*!
 * Gives y distance (how far away) in inches
 */
const double y_distance_to_tape_from_camera(double measured_length)
{
	return (real_length*focal_length)/(measured_length);
}

double camera_angle_to(double x_center_pos)
{
	double from_center = x_center_pos - (res_x/2);
	double angle_from_camera = from_center*angle_coeffecient;
	return angle_from_camera;
}

//const double to_
/*!
 * Gives x distance (how right/left) in inches
 */
const double x_distance_to_tape(double measured_length, double x_center_pos)
{
	double angle_from_camera=camera_angle_to(x_center_pos);

	return y_distance_to_tape_from_camera(measured_length) * tan(angle_from_camera);

}

const double angle_from_bot(double x_dist, double y_dist_from_camera)
{
	double y_dist = y_dist_from_camera + camera_location_inches_from_center;
	return atan(x_dist/y_dist);
}





double get_unix_time()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);

    return static_cast<double>(tv.tv_sec)+(static_cast<double>(tv.tv_usec)/(static_cast<double>(10e6))); //Add the microseconds divded by 10e6 to end up with seconds.microseconds
    //Static casting intensifies
}





Mat draw_rects_on_image(Mat const & in_image, std::vector<std::vector<cv::Point>> rect_vect)
{
	Mat out_image = in_image;

	double amnt_color_increase_per_image = 256.f/rect_vect.size();

	double cur_color = 255.f;
	bool alt_flash = false;

	for (auto rectangle: rect_vect)
	{
		/*rectangle.clear();
		rectangle.push_back(cv::Point(10,10));
		rectangle.push_back(cv::Point(10,100));
		rectangle.push_back(cv::Point(100,100));
		rectangle.push_back(cv::Point(100,10));
		rectangle.push_back(cv::Point(10,10));*/ //<<test shape fill
		cv::polylines(out_image,rectangle, false, alt_flash? 256.f-cur_color : cur_color, 2);	
		//cur_color+= amnt_color_increase_per_image;
		//alt_flash = !alt_flash;
	}

	return out_image;
}

cv::Point filter_points(std::function<cv::Point(cv::Point,cv::Point)> get_best_point, std::vector<cv::Point> point_vect)
{
	assert((point_vect.size() >= 1));
	cv::Point best = point_vect[0];
	for (auto point : point_vect)
	{
		best = get_best_point(best,point);
	}
	return best;
}
cv::Point get_leftmost_point(std::vector<cv::Point> point_vec)
{
	return filter_points([](cv::Point a, cv::Point b)
	{ 
		return (a.x < b.x)? a : b;
	},
	point_vec);
}
cv::Point get_rightmost_point(std::vector<cv::Point> point_vec)
{
	return filter_points([](cv::Point a, cv::Point b)
	{ 
		return (a.x > b.x)? a : b;
	},
	point_vec);
}
/*!
 * This generates a value that can be used to compare 2 points on x and y simultaniously.
 */
double score_point(cv::Point point, bool wantRight, bool wantBottom)
{
	if (!wantRight)
	{
		point.x *= -1;
	}
	if (!wantBottom)
	{
		point.y *= -1;
	}
	return point.x + point.y;
}
//Structure:
//0 - top left
//1 - top right
//2 - bottom right
//3 - bottom left
std::vector<cv::Point> get_rect_corners(std::vector<cv::Point> point_vec)
{
	std::vector<cv::Point> result;
	result.push_back(filter_points([](cv::Point a, cv::Point b)
	{
		if (score_point(a,false,false) > score_point(b,false,false))
		{
			return a;
		}
		return b;
	},
	point_vec));
	result.push_back(filter_points([](cv::Point a, cv::Point b)
	{
		if (score_point(a,true,false) > score_point(b,true,false))
		{
			return a;
		}
		return b;
	},
	point_vec));
	result.push_back(filter_points([](cv::Point a, cv::Point b)
	{
		if (score_point(a,true,true) > score_point(b,true,true))
		{
			return a;
		}
		return b;
	},
	point_vec));
	result.push_back(filter_points([](cv::Point a, cv::Point b)
	{
		if (score_point(a,false,true) > score_point(b,false,true))
		{
			return a;
		}
		return b;
	},
	point_vec));
	return result;
}
class compare_rects_leftmost_lower
{
	public:
		bool operator()(std::vector<cv::Point> const & lhs, std::vector<cv::Point> const & rhs)
		{
			cv::Point lhs_l = get_leftmost_point(lhs);	
			cv::Point rhs_l = get_leftmost_point(rhs);	

			return lhs_l.x < rhs_l.x;
		}
};
class compare_rects_closest_to_point_x
{
	int point;
	int x_dist(cv::Point p)
	{
		return std::abs(p.x-point);
	}
	public:
		compare_rects_closest_to_point_x(int x)
		{
			point = x;
		}
		bool operator()(std::vector<cv::Point> const & lhs, std::vector<cv::Point> const & rhs)
		{
			int lhs_l = std::min(x_dist(get_leftmost_point(lhs)),x_dist(get_rightmost_point(lhs)));
			int rhs_l = std::min(x_dist(get_leftmost_point(rhs)),x_dist(get_rightmost_point(rhs)));

			return lhs_l < rhs_l;
		}
};

void test_point_filter()
{
	std::vector<cv::Point> t1;
	t1.push_back({100,0});
	t1.push_back({200,0}); // highest
	t1.push_back({0,0});
	t1.push_back({-27,0});
	t1.push_back({50,0});

	assert((get_rightmost_point(t1) == cv::Point{200,0}));
	assert((get_leftmost_point(t1) == cv::Point{-27,0}));

}


bool is_right_rectangle(std::vector<cv::Point> rect1, std::vector<cv::Point> rect2)
{

	auto leftmost_on_1 = get_leftmost_point(rect1);
	auto leftmost_on_2 = get_leftmost_point(rect2);

	return leftmost_on_1.x > leftmost_on_2.x;
}

cv::Point get_average_pt(cv::Point a, cv::Point b)
{
	return cv::Point((a.x+b.x)/2,(a.y+b.y)/2);
}
cv::Point center_point_on_angled_rects(std::vector<std::vector<cv::Point>> rect_vect)
{
	//determine which is which
	std::vector<cv::Point> left;
	std::vector<cv::Point> right;



	if (is_right_rectangle(rect_vect[0],rect_vect[1]))
	{
		right = rect_vect[0];
		left = rect_vect[1];
	}
	else
	{
		right = rect_vect[1];
		left = rect_vect[0];
	}

	auto closest_to_center_left = get_rightmost_point(left);
	auto closest_to_center_right = get_leftmost_point(right);

	return get_average_pt(closest_to_center_left,closest_to_center_right);	
}
double get_area(std::vector<cv::Point> rect)
{
	return ((rect[0].x*rect[1].y - rect[1].x*rect[0].y) + (rect[1].x * rect[2].y - rect[2].x * rect[1].y) + (rect[2].x  * rect[3].y - rect[3].x * rect[2].y) + (rect[3].x * rect[0].y - rect[0].x * rect[3].y))/2.f;
}
class compare_rects_biggest
{
	public:
		compare_rects_biggest()
		{
		}
		bool operator()(std::vector<cv::Point> const & lhs, std::vector<cv::Point> const & rhs)
		{
			return get_area(get_rect_corners(lhs)) > get_area(get_rect_corners(rhs));
		}
};


cv::Point get_center(std::vector<cv::Point> rect)
{
	//center can be obtained simply by averaging all of the points.
	double tot_x,tot_y = 0;
	for (auto this_pt : rect)
	{
		tot_x += this_pt.x;
		tot_y += this_pt.y;
	}
	double size = static_cast<double>(rect.size());
	return cv::Point{tot_x/size,tot_y/size};
}


constexpr double px_closeness(double px, double target)
{
	return 1.f-( (std::abs(px-target)/(target)) ); // 1 - (percent difference) = percent similar
}
static_assert(px_closeness(1.25,1)==.75, "px_closeness is incorrect");
static_assert(px_closeness(.75,1)==.75, "px_closeness is incorrect");

double channel_closeness(Vec3b const & channel, Vec3b const & target, Vec3f const & channel_weights)
{
	//printf(" ch %d %d %d\n",channel[0],channel[1],channel[2]);
	//printf(" target %d %d %d\n",target[0],target[1],target[2]);
	//average the closeness together	
	double ch0 = px_closeness(channel[0], target[0]);
	double ch1 = px_closeness(channel[1], target[1]);
	double ch2 = px_closeness(channel[2], target[2]);

	//printf("0 1 2 %f %f %f\n",ch0,ch1,ch2);

	return (ch0*channel_weights[0])+(ch1*channel_weights[1])+(ch2*channel_weights[2]);
}


Mat closeness_rating(Mat const & image, Vec3b const & target, Vec3f channel_weights, float min_percent)
{
	Mat final_img(image.size(),CV_8UC1);
#pragma omp parallel
	for (size_t row = 0; row < image.rows; row++)
	{
#pragma omp parallel
		for( size_t col = 0; col < image.cols; col++)
		{
			auto px = image.at<Vec3b>(row,col);
			double rating = channel_closeness(px,target,channel_weights);
			if (rating < min_percent)
			{
				rating = 0.f;
			}
			//printf("rating %f\n",rating);
			final_img.at<unsigned char>(row,col) = static_cast<unsigned char>((rating)*255.f);
		}
	}
	return final_img;
}


/*!
 * Write to the socket a serialization_state of constant size (12 bytes) which contains a 64-bit int that is the size of the following
 * serialization state (data) to be sent, then send the data in data.
 */
void send_serialization_data(serialization::serialization_state const & data, network::socket & socket)
{
	serialization::serialization_state size_state(1);
	serialization::serialize(static_cast<serialization::size_tp>(data.bufferUsed),size_state); //serialize the size of the first buffer (will always come out as 12 bytes)

	socket.write(std::string(size_state.buffer,12)); // inform them of the size of the next one. the "+4" is to skip the version uint32_t present on all serialization_state's.
	socket.write(std::string(data.buffer,data.bufferUsed)); //send the data over.
}





class file_not_found : public std::exception
{
	public:
		std::string s_msg;
		const char* msg;
		file_not_found(std::string filename)
		{
			s_msg = (std::string("File ") + filename + " not found");
			msg = s_msg.c_str();
		}
		virtual const char* what() const noexcept override 
		{
			return msg;
		}
};

class distortion
{
	public:
		Mat distortion_coeffecients;
		Mat camera_matrix;
		Mat map1, map2;
		int width, height;
		distortion(std::string filename)
		{
			cv::FileStorage fs(filename.c_str(), FileStorage::READ);
			if (!fs.isOpened())
			{
				throw file_not_found(filename);
			}

			//fs >> "Distortion_Coefficients" >> distortion_coeffecients;
			fs["Distortion_Coefficients"] >> distortion_coeffecients;
			fs["Camera_Matrix"] >> camera_matrix;
			fs["image_Width"] >> width;
			fs["image_Height"] >> height;

			auto image_size = cv::Size2d(width,height);

			initUndistortRectifyMap(camera_matrix,distortion_coeffecients, Mat(),
            getOptimalNewCameraMatrix(camera_matrix, distortion_coeffecients, image_size, 1, image_size, 0), 
            image_size, CV_16SC2, map1, map2);
		}
		cv::Mat undistort(cv::Mat const & in)
		{
			Mat out;
			remap(in, out, map1, map2, INTER_LINEAR);
			return out;
		}

};

struct angle_and_distance
{
	double angle;
	double distance;
};
angle_and_distance get_robot_angle_and_distance(std::vector<std::vector<cv::Point>> rects)
{
	angle_and_distance returnval;
	auto rect = rects[0]; //use the bigger rectangle
	auto corners = get_rect_corners(rect);

	//this will not be accurate to the actual length, HOWEVER it does preserve ratios -- meaning it can be used to find the distance to the rectangle.
	double length = sqrt(get_area(corners));

	cv::Point center = center_point_on_angled_rects(rects);
	double pre_y_dist = y_distance_to_tape_from_camera(length);
	double x_dist = x_distance_to_tape(length, center.x);

	returnval.angle = angle_from_bot(x_dist,pre_y_dist);
	returnval.distance = pre_y_dist + camera_location_inches_from_center;

	return returnval;

}

int main(int argument_count, char** arguments)
{

#if __BYTE_ORDER == __BIG_ENDIAN
	printf("big\n");
#else
	printf("little\n");
#endif


	flag_interpreter::results_t flags;

	auto shorthands = flag_interpreter::shorthands_t{
		{"c","camera"}, {"p","picture_path"},{"v","video_path"},{"h","help"},{"g","gui"},{"t","time"},{"ds","downsample"},{"sa","space_advance"},{"s","server"},{"r","rectification"}
	};
	try 
    {   
        flags= flag_interpreter::process(argument_count,arguments,shorthands);
    }   
    catch (std::exception const & e)
    {   
        std::cout << std::string(e.what()) <<std::endl;
        exit(1);
    }   

	bool has_picture_method = false;
	bool display_gui = false;
	bool measure_time = false;
	bool downsample_gui = false;

	bool write_picture = false;

	bool space_advance = false;

	std::optional<VideoCapture> camera;
	std::optional<cv::Mat> picture;
	std::optional<std::string> video_path;
	std::optional<network::socket> server_socket;
	std::optional<network::socket> client_socket;

	std::optional<cv::VideoWriter> video_writer;

	std::optional<distortion> my_distortion;



	
	//TODO - ugly code. needs to be a template function
	for (auto& flag : flags)
	{
		if (flag.flag == "camera")
		{
			if (has_picture_method)
			{
				throw std::runtime_error("Already specified picture/video input method."); 
			}
			has_picture_method = true;
			if (flag.option.size() != 1)
			{
				throw std::runtime_error("invalid options count for camera port");
			}
			try
			{
				auto camera_port = std::stoi(flag.option[0]);
				
				camera = VideoCapture(camera_port); // open the default camera;
				if(!camera->isOpened())  // check if we succeeded
				{
					throw std::runtime_error("camera doesn't exist");
				}
				camera->set(cv::CAP_PROP_FRAME_WIDTH,res_x);
				camera->set(cv::CAP_PROP_FRAME_HEIGHT,res_y);
				camera->set(cv::CAP_PROP_AUTO_EXPOSURE,0);
				camera->set(cv::CAP_PROP_EXPOSURE,-10);
				printf("Camera ready.\n");
			}
			catch ( std::exception const &)
			{
				throw std::runtime_error("Invalid argument for camera (needs integer value)");
			}
		}
		else if (flag.flag == "video_path")
		{
			if (has_picture_method)
			{
				throw std::runtime_error("Already specified picture/video input method."); 
			}
			has_picture_method = true;
			if (flag.option.size() != 1)
			{
				throw std::runtime_error("invalid options count for camera port");
			}
			try
			{
				camera = VideoCapture(flag.option[0]);
				if (!camera->isOpened())
				{
					throw std::runtime_error("Video file doesn't exist");
				}
				printf("Video opened\n");
			}
			catch (std::exception const &)
			{
				throw std::runtime_error("Invalid argument for video");
			}
		}
		else if (flag.flag == "rectification")
		{
			if (flag.option.size() != 1)
			{
				throw std::runtime_error("Need configuration file (generated by the program HERE: https://docs.opencv.org/2.4/doc/tutorials/calib3d/camera_calibration/camera_calibration.html ) to rectify.");
			}
			my_distortion = {distortion(flag.option[0])};
		}
		else if (flag.flag == "picture_path")
		{
			if (has_picture_method)
			{
				throw std::runtime_error("Already specified picture/video input method."); 
			}
			has_picture_method = true;
			if (flag.option.size() != 1)
			{
				throw std::runtime_error("invalid options count for picture_path file");
			}
			auto picture_path = flag.option[0];
			picture = imread(picture_path);
		}
		else if (flag.flag == "video_path")
		{
			if (has_picture_method)
			{
				throw std::runtime_error("Already specified picture/video input method."); 
			}
			has_picture_method = true;
			if (flag.option.size() != 1)
			{
				throw std::runtime_error("invalid options count for video_path file");
			}
			video_path = flag.option[0];
		}
		else if (flag.flag == "gui")
		{
			display_gui = true;
		}
		else if (flag.flag == "time")
		{
			measure_time = true;
		}
		else if (flag.flag == "help")
		{
			//TODO this should be a generic function to generate this string from descriptions and shorthands_t
			printf("Camera tracking help\n--help\t-h\tShows this dialog.\n--picture_path\t-p\tSpecify a static picture file to be used for camera tracking(probably for debugging)\n--video_path\t-v\tSpecify a static video file to be used for camera tracking\n--camera\t-c\tSpecify a camera port for camera tracking (0 is the default system camera)\n");
			exit(0);
		}
		else if (flag.flag == "downsample")
		{
			downsample_gui = true; 
		}
		else if (flag.flag == "fakeserver")
		{
			network::socket server(5667);

			server.bind_as_server();
			auto client = server.listen_for_client();
			while(true)
			{
				serialization::serialization_state state(1);
				serialization::serialize((float)0.f,state);

				send_serialization_data(state,client);
			}
		}
		else if (flag.flag == "server")
		{
			printf("---REAL CAMERA TRACKING SERVER---\n");
			bool got_sock = false;
			for (unsigned short i = 5667; i < 5667+11; i++)
			{
				try
				{
					server_socket = network::socket(i);
					server_socket->bind_as_server();
					printf("Bound to port %d\n",i);
					got_sock = true;
					break;
				}
				catch (std::exception const &)
				{
					printf("caught\n");
				}
			}
			if (!got_sock)
			{
				throw std::runtime_error("Couldn't get any network socket. This is caused by the roborio disconnecting and forcing the jetson to spin up a new server more than 10 times.");
			}
			//Wait until finished parsing flags to listen for client -- so that we don't block wait while other flags are trying to be parsed.

		}
		else if (flag.flag == "write_picture")
		{
			write_picture=true;
		}
		else if (flag.flag == "write_video")
		{
			video_writer = cv::VideoWriter("outvideo.avi",cv::VideoWriter::fourcc('X','V','I','D'),25.f,cv::Size2d{static_cast<double>(res_x),static_cast<double>(res_y)},true);
		}
		else if (flag.flag == "space_advance")
		{
			space_advance = true;
		}
		else
		{
			throw std::runtime_error("Unrecognized command "+flag.flag);
		}
	}
	if (!has_picture_method)
	{
		throw std::runtime_error("Failed to specify picture or video input method to track. (-h / --help)");
	}


	//We do this outside of the flag parsing because it causes a block wait.
	if (server_socket.has_value())
	{
		client_socket =  server_socket->listen_for_client();
		printf("BEGIN SENDING DATA\n");
	}











	double total_frame_time = 0.f;
	double total_camera_capture_time = 0.f;
	int frame_times_sampled = 0;
	int total_frames = 0;

	


    Mat edges;
	if (display_gui)
	{
		namedWindow("edges",1);
	}

	test_point_filter();


	
	size_t frames = 0;
    while(true)
    {
		double start_time;
		if (measure_time)
		{
			start_time = get_unix_time();
		}

        Mat frame;
		if (camera.has_value())
		{
			*camera >> frame; // get a new frame from camera
		}
		else if (picture.has_value())
		{
			picture->copyTo(frame);
		}

		if (write_picture)
		{	
			if (frames >= 100)
			{
				imwrite("OUT_PIC.jpg",frame);	
				exit(0);
			}
		}
		if (video_writer.has_value())
		{
			video_writer->write(frame);	
		}

		//Process
		
		if (my_distortion.has_value())
		{
			//rectify
			frame = my_distortion->undistort(frame);
		}


		
		double time_took_to_take_picture;
		if (measure_time)
		{
			time_took_to_take_picture = get_unix_time()-start_time;
		}
		//frame = imread("red_test_img.png");
		
		//Mat mask(frame.size(), CV_8UC3, Scalar(255,255,255));
		/*unsigned char max_val;
		for (size_t row = 0; row < frame.rows; row++)
		{
			for( size_t col = 0; col < frame.cols; col++)
			{
				mask.at<Vec3b>(row,col) = Vec3b(210,255,0);
			}
		}
		printf("maxval %d\n",max_val);
		*/


		//0: blue
		//1: green
		//2: red


		auto in = Vec3b{36,102,4};//Vec3b{17,112,7};//Vec3b{218,255,123};//Vec3b{232,255,126};
		auto weights = Vec3f{.20,.6,.20};
		auto correct_pxs = closeness_rating(frame,in,weights,.6);

		Mat filtered;

		GaussianBlur(correct_pxs, filtered, Size(7,7), 1.5, 1.5);



		std::vector<std::vector<cv::Point>> out_contours;
		std::vector<cv::Vec4i> out_hiearchy;

		findContours(filtered, out_contours, out_hiearchy, RETR_LIST, CHAIN_APPROX_TC89_L1);

		std::vector<std::vector<cv::Point>> good_shapes = {};


		//std::sort(out_contours.begin(), out_contours.end(), compare_rects_leftmost_lower());
		std::sort(out_contours.begin(), out_contours.end(), compare_rects_biggest());//compare_rects_closest_to_point_x(res_x/2));
		for (size_t i = 0; i < 2; i++)
		{
			if (out_contours.begin()+i == out_contours.end())
			{
				break;
			}
			good_shapes.push_back(out_contours[i]);
		}


		
		/*for (auto i : good_shapes)
		{
			for (auto o : i)
			{
				printf("point xy %d, %d\n",o.x,o.y);
			}
			printf(";\n");
		}*/
		printf("countr # %d\n",good_shapes.size());

		

		std::optional<cv::Point> center;
		if (good_shapes.size() >= 2)
		{
			center = center_point_on_angled_rects(good_shapes);
		}

		if (display_gui)
		{
			auto finalimg = draw_rects_on_image(frame, good_shapes);
			if (center.has_value())
			{
				circle(finalimg, center.value(), 30, Scalar(100,200,255),5);
				
			}
			
			if (downsample_gui)
			{
				cv::Mat resized;
				cv::resize(finalimg,resized,cv::Size(320,240),0,0,cv::INTER_NEAREST);
				imshow("edges",resized);
			}
			else
			{
				imshow("edges",finalimg);//channel_closeness({210,255,0}, frame));
			}
			while(true)
			{
				int waitkey_val = waitKey(30);
				if (space_advance)
				{
					if (waitkey_val != 32)
					{
						continue;
					}
				}	
				break;
			}
		}

		if (center.has_value())
		{
			auto angle_and_distance_val = get_robot_angle_and_distance(good_shapes);
			printf("Angle: %f deg\n",angle_and_distance_val.angle*to_deg);
			if (client_socket.has_value()) //if there is a client connected
			{
				serialization::serialization_state state(1);
				serialization::serialize(angle_and_distance_val.angle,state);
				//TODO update client & server to send distance

				send_serialization_data(state,client_socket.value());
			}

		}
		printf("showed img\n");

		if (measure_time)
		{
			double time_took = get_unix_time()-start_time;

			total_frame_time += time_took;
			total_camera_capture_time += time_took_to_take_picture;
			frame_times_sampled++;
			printf("this frame: %f FPS\navg: %f FPS over %d samples\n",1/time_took,frame_times_sampled/total_frame_time,frame_times_sampled);
			printf("camera: %f FPS\navg: %f FPS\n",1/time_took_to_take_picture, frame_times_sampled/total_camera_capture_time);
		}
		frames++;
    }
    // the camera will be deinitialized automatically in video_pathCapture destructor
    return 0;
}

		/*Mat filtered1;
		adaptiveThreshold(planes[0],filtered1,255,ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, 101);
		*/
