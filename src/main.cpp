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


#include "opencv2/opencv.hpp"
#include "opencv2/imgproc.hpp"

#include "cli/flag_interpreter.hpp"
#include "network/network.hpp"
#include "serialization/serialization.hpp"

using namespace cv;



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



constexpr double px_closeness(double px, double target)
{
	return 1.f-( (std::abs(px-target)/(target)) ); // 1 - (percent difference) = percent similar
}
static_assert(px_closeness(1.25,1)==.75, "px_closeness is incorrect");
static_assert(px_closeness(.75,1)==.75, "px_closeness is incorrect");

double channel_closeness(Vec3b const & channel, Vec3b const & target)
{
	//printf(" ch %d %d %d\n",channel[0],channel[1],channel[2]);
	//printf(" target %d %d %d\n",target[0],target[1],target[2]);
	//average the closeness together	
	double ch0 = px_closeness(channel[0], target[0]);
	double ch1 = px_closeness(channel[1], target[1]);
	double ch2 = px_closeness(channel[2], target[2]);

	//printf("0 1 2 %f %f %f\n",ch0,ch1,ch2);

	return (ch0+ch1+ch2)/3.f;
}

Mat closeness_rating(Mat const & image, Vec3b const & target)
{
	Mat final_img(image.size(),CV_8UC1);
#pragma omp parallel
	for (size_t row = 0; row < image.rows; row++)
	{
#pragma omp parallel
		for( size_t col = 0; col < image.cols; col++)
		{
			auto px = image.at<Vec3b>(row,col);
			double rating = channel_closeness(px,target);
			if (rating < .90f)
			{
				rating = 0.f;
			}
			//printf("rating %f\n",rating);
			final_img.at<unsigned char>(row,col) = static_cast<unsigned char>((rating)*255.f);
		}
	}
	return final_img;
}


enum class pull_mode
{
	camera,
	image,
	video_path
};


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

int main(int argument_count, char** arguments)
{

#if __BYTE_ORDER == __BIG_ENDIAN
	printf("big\n");
#else
	printf("little\n");
#endif


	flag_interpreter::results_t flags;

	auto shorthands = flag_interpreter::shorthands_t{
		{"c","camera"}, {"p","picture_path"},{"v","video_path"},{"h","help"},{"g","gui"},{"t","time"},{"ds","downsample"}, 
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

	std::optional<int> camera_port;
	std::optional<std::string> picture_path;
	std::optional<std::string> video_path;
	
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
				camera_port = std::stoi(flag.option[0]);
			}
			catch ( std::exception const &)
			{
				throw std::runtime_error("Invalid argument for camera (needs integer value)");
			}
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
			picture_path = flag.option[0];
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
		else if (flag.flag == "sockettestserver")
		{
			network::socket server(5667);

			server.bind_as_server();

			auto client = server.listen_for_client();

			printf("recieved msg %s\n",client.read(14).c_str());


			uint32_t data = 0;
			while(true)
			{
				serialization::serialization_state state(1);
				serialization::serialize(data,state);
				serialization::serialize((float)data,state);

				send_serialization_data(state,client);
				data++;
			}


			return 0;
		}
		else if (flag.flag == "sockettestclient")
		{
			network::socket client(5667);

			client.connect_to_server("127.0.0.1");
			client.write("Hello, World!!\n");

			while(true)
			{
				//get size
				std::string buf_size = client.read(12);

				auto unser_size_state = serialization::unserialization_state(buf_size.data());

				//alloc
				auto need_to_read = static_cast<size_t>(serialization::unserialize<serialization::size_tp>(unser_size_state));
				need_to_read = 8;
				printf("v %d ntr %lu\n",unser_size_state.serializedVersion,need_to_read);
				auto buffer = client.read(need_to_read);
				auto unser_state = serialization::unserialization_state(buffer.data());

				for (size_t i = 0; i < need_to_read; i++)
				{
					printf("this byte %d\n",(unsigned char) buffer[i]);
				}

				uint32_t result = serialization::unserialize<uint32_t>(unser_state);

				printf("Got number %d\n",result);
			}




			//printf("recieved msg %s\n",client.read(12).c_str());
			return 0;
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











    VideoCapture cap;
	Mat picture;


	double total_frame_time = 0.f;
	double total_camera_capture_time = 0.f;
	int frame_times_sampled = 0;
	int total_frames = 0;

	
	if (camera_port.has_value())
	{
		cap = VideoCapture(camera_port.value()); // open the default camera;
		if(!cap.isOpened())  // check if we succeeded
		{
			throw std::runtime_error("camera doesn't exist");
		}
		cap.set(cv::CAP_PROP_FRAME_WIDTH,1280);
		cap.set(cv::CAP_PROP_FRAME_HEIGHT,720);
	}
	else if (picture_path.has_value())
	{
		picture = imread(picture_path.value());
	}


    Mat edges;
	if (display_gui)
	{
		namedWindow("edges",1);
	}

	test_point_filter();


	
    while(true)
    {
		double start_time;
		if (measure_time)
		{
			start_time = get_unix_time();
		}

		//cap.set(CAP_PROP_AUTO_EXPOSURE, 1);
		//cap.set(CAP_PROP_EXPOSURE, 000);
		
        Mat frame;
		if (camera_port.has_value())
		{
			cap >> frame; // get a new frame from camera
		}
		else if (picture_path.has_value())
		{
			picture.copyTo(frame);
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


		auto in = Vec3b{232,255,126};
		auto correct_pxs = closeness_rating(frame,in);

		Mat filtered;

		GaussianBlur(correct_pxs, filtered, Size(7,7), 1.5, 1.5);



		std::vector<std::vector<cv::Point>> out_contours;
		std::vector<cv::Vec4i> out_hiearchy;

		findContours(filtered, out_contours, out_hiearchy, RETR_LIST, CHAIN_APPROX_TC89_L1);


		std::sort(out_contours.begin(), out_contours.end(), compare_rects_leftmost_lower());


		
		/*for (auto i : out_contours)
		{
			for (auto o : i)
			{
				printf("point xy %d, %d\n",o.x,o.y);
			}
			printf(";\n");
		}*/
		printf("countr # %d\n",out_contours.size());

		



		if (display_gui)
		{
			auto finalimg = draw_rects_on_image(frame, out_contours);
			if (out_contours.size() >= 2)
			{
				cv::Point center = center_point_on_angled_rects(out_contours);
				circle(finalimg, center, 30, Scalar(100,200,255),5);
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
			if(waitKey(30) >= 0) printf("k\n");
		}
		//imwrite("redchntest",planes[0]);
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
    }
    // the camera will be deinitialized automatically in video_pathCapture destructor
    return 0;
}

		/*Mat filtered1;
		adaptiveThreshold(planes[0],filtered1,255,ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, 101);
		*/
