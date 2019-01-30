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


#include "opencv2/opencv.hpp"
#include "opencv2/imgproc.hpp"

#include "cli/flag_interpreter.hpp"
using namespace cv;




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
	for (size_t row = 0; row < image.rows; row++)
	{
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

int main(int argument_count, char** arguments)
{



	flag_interpreter::results_t flags;

	auto shorthands = flag_interpreter::shorthands_t{
		{"c","camera"}, {"p","picture_path"},{"v","video_path"},{"h","help"}
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
		else if (flag.flag == "help")
		{
			//TODO this should be a generic function to generate this string from descriptions and shorthands_t
			printf("Camera tracking help\n--help\t-h\tShows this dialog.\n--picture_path\t-p\tSpecify a static picture file to be used for camera tracking(probably for debugging)\n--video_path\t-v\tSpecify a static video file to be used for camera tracking\n--camera\t-c\tSpecify a camera port for camera tracking (0 is the default system camera)\n");
			exit(0);
		}
			
			
	}
	if (!has_picture_method)
	{
		throw std::runtime_error("Failed to specify picture or video input method to track. (-h / --help)");
	}











    VideoCapture cap;
	Mat picture;

	
	if (camera_port.has_value())
	{
		cap = VideoCapture(camera_port.value()); // open the default camera;
		if(!cap.isOpened())  // check if we succeeded
		{
			throw std::runtime_error("camera doesn't exist");
		}
	}
	else if (picture_path.has_value())
	{
		picture = imread(picture_path.value());
	}


    Mat edges;
    namedWindow("edges",1);

	test_point_filter();

    while(true)
    {
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


		Mat planes[3];
		split(frame,planes);
		//0: blue
		//1: green
		//2: red


		/*
		Mat red_inv;
		bitwise_not(planes[2],red_inv);
		Mat blue_inv;
		bitwise_not(planes[0],blue_inv);

		Mat _tmp;

		Mat green_only;

		planes[1].copyTo(green_only);
		*/
		/*_tmp = planes[1] - planes[2];
		green_only = _tmp - planes[0];*/

		//Mat green_only = green_only_negatives;//planes[1];

		//planes[0] -= planes[1];
		//planes[0] -= planes[2];


        /*cvtColor(frame, edges, CV_BGR2GRAY);
#define L 21
		for (size_t i = 1; i < L; i+=2)
		{
			medianBlur(edges,edges,i);
		}
        GaussianBlur(edges, edges, Size(7,7), 1.5, 1.5);
        Canny(edges, edges, 10, 30, 3);*/


		auto in = Vec3b{232,255,126};
		auto correct_pxs = closeness_rating(frame,in);

		Mat filtered;

		Mat after_thresh;
		threshold(correct_pxs, after_thresh, 100,255, THRESH_TOZERO);

        GaussianBlur(after_thresh, filtered, Size(7,7), 1.5, 1.5);



		std::vector<std::vector<cv::Point>> out_contours;
		std::vector<cv::Vec4i> out_hiearchy;

		findContours(filtered, out_contours, out_hiearchy, RETR_LIST, CHAIN_APPROX_TC89_L1);


		
		/*for (auto i : out_contours)
		{
			for (auto o : i)
			{
				printf("point xy %d, %d\n",o.x,o.y);
			}
			printf(";\n");
		}*/
		printf("countr # %d\n",out_contours.size());

		auto finalimg = draw_rects_on_image(frame, out_contours);

 
		if (out_contours.size() >= 2)
		{
			cv::Point center = center_point_on_angled_rects(out_contours);

			circle(finalimg, center, 30, Scalar(100,200,255),5);
		}



        imshow("edges",finalimg);//channel_closeness({210,255,0}, frame));
		//imwrite("redchntest",planes[0]);
		printf("showed img\n");
        if(waitKey(30) >= 0) printf("k\n");
		usleep(16);
    }
    // the camera will be deinitialized automatically in video_pathCapture destructor
    return 0;
}

		/*Mat filtered1;
		adaptiveThreshold(planes[0],filtered1,255,ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 15, 101);
		*/
