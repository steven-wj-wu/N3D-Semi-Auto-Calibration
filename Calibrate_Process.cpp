#include "Calibrate_Process.h"
#include <chrono>
#include <ctime>



int  D3Calibrate_lib::set_image_by_path(string path) {
	current_image.image = imread(path);
	cvtColor(current_image.image, current_image.hsv_image, COLOR_BGR2HSV);
	current_image.height = current_image.image.rows;
	current_image.width = current_image.image.cols;

	if (!(current_image.height && current_image.width)) {
		return 0;
	}

	return 1;
}
int D3Calibrate_lib::set_image_by_camera(Mat source) {
current_image.image = source;
cvtColor(current_image.image, current_image.hsv_image, COLOR_BGR2HSV);
current_image.height = current_image.image.rows;
current_image.width = current_image.image.cols;

if (!(current_image.height && current_image.width)) {
	return 0;
}

return 1;
}
int D3Calibrate_lib::set_image_by_dual_camera(Mat source_left,Mat source_right) {

	current_image_left.image = source_left.clone();
	current_image_right.image =source_right .clone();
	cvtColor(source_left, current_image_left.hsv_image, COLOR_BGR2HSV);
	cvtColor(source_right, current_image_right.hsv_image, COLOR_BGR2HSV);
	current_image_left.height = current_image_left.image.rows;
	current_image_left.width = current_image_left.image.cols;
	current_image_right.height = current_image_right.image.rows;
	current_image_right.width = current_image_right.image.cols;

	if (current_image_left.image.empty() || current_image_right.image.empty()) {
		return 0;
	}

	return 1;
}
void D3Calibrate_lib::image_size(int* height, int* width, int* channel) {
	*height = current_image.height;
	*width = current_image.width;
	*channel = current_image.image.channels();
}
void  D3Calibrate_lib::get_current_image_pixelRGB_by_position(int* r, int* g, int* b, int index) {
	*b = (int)current_image.image.data[3 * index + 0];
	*g = (int)current_image.image.data[3 * index + 1];
	*r = (int)current_image.image.data[3 * index + 2];
}
void  D3Calibrate_lib::get_current_image_pixelHSV_by_position(int* h, int* s, int* v, int index) {
	*h = (int)current_image.hsv_image.data[3 * index + 0];
	*s = (int)current_image.hsv_image.data[3 * index + 1];
	*v = (int)current_image.hsv_image.data[3 * index + 2];
}
void D3Calibrate_lib::Ref_RG_generate_by_default(int* red_h, int* red_s, int* green_h, int* green_s) {
	Mat tmp_img_red(1, 1, CV_8UC3, Scalar(DEFAUL_REF_Red_B, DEFAUL_REF_Red_G, DEFAUL_REF_Red_R));
	Mat red_hsv;
	cvtColor(tmp_img_red, red_hsv, COLOR_BGR2HSV);

	Mat tmp_img_green(1, 1, CV_8UC3, Scalar(DEFAUL_REF_GREEN_B, DEFAUL_REF_GREEN_G, DEFAUL_REF_GREEN_R));
	Mat green_hsv;
	cvtColor(tmp_img_green, green_hsv, COLOR_BGR2HSV);

	*red_h = (int)red_hsv.data[0];
	*red_s = (int)red_hsv.data[1];
	*green_h = (int)green_hsv.data[0];
	*green_s = (int)green_hsv.data[1];
}
double  D3Calibrate_lib::get_RG_Ratio() {
	/*
	int ref_red_h = ref_red_hsv[0];
	int ref_red_s = ref_red_hsv[1];
	int ref_green_h = ref_green_hsv[0];
	int ref_green_s = ref_green_hsv[1];
	*/
	int ref_red_h = reference[0].hsv[0];
	int ref_red_s = reference[0].hsv[1];
	int ref_green_h = reference[1].hsv[0];
	int ref_green_s = reference[1].hsv[1];

	int height = current_image.height;
	int width = current_image.width;
	Mat hsv = current_image.hsv_image;

	int h, s;
	int index = 0;
	double d_g, d_r;
	double d_sum = 0.0;
	double RG_ratio = 0.0;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			index = i * hsv.cols + j;
			h = (int)hsv.data[3 * index + 0];
			s = (int)hsv.data[3 * index + 1];

			//compare two h value difference (0-180-0)
			if (abs(h - ref_green_h) > (180 - ref_green_h + h)) {
				d_g = sqrt((180 - ref_green_h + h) * (180 - ref_green_h + h) + (s - ref_green_s) * (s - ref_green_s));
			}
			else {
				d_g = sqrt((h - ref_green_h) * (h - ref_green_h) + (s - ref_green_s) * (s - ref_green_s));
			}

			if (abs(h - ref_red_h) > (180 - ref_red_h + h)) {
				d_r = sqrt((180 - ref_red_h + h) * (180 - ref_red_h + h) + (s - ref_red_s) * (s - ref_red_s));
			}
			else {
				d_r = sqrt((h - ref_red_h) * (h - ref_red_h) + (s - ref_red_s) * (s - ref_red_s));
			}

			d_sum += min(d_g, d_r);
		}
	}

	RG_ratio = d_sum / (((double)height - 1) * ((double)width - 1));

	return RG_ratio;
}
long double  D3Calibrate_lib::get_RG_Ratio_single_color(int side ,bool is_weight) {
    //? side 0 :(red)  1 :(green)
	float ref_h =(float) reference[side].hsv[0];
	float ref_s = (float)reference[side].hsv[1];
	int height, width;
	Mat hsv;

	if (side == 0) {
        //左眼與參考紅計算距離
        height = current_image_left.height;
        width = current_image_left.width;
        hsv = current_image_left.hsv_image.clone();
	}
	else if (side == 1) {
        //右眼與參考綠計算距離
        height = current_image_right.height;
        width = current_image_right.width;
        hsv = current_image_right.hsv_image.clone();
	}
	else {
		return -1;
	}

	long double h, s,v;
	int index = 0;
	float valid_pixel_num = 0;
	long double d_g;
	long double d_sum = 0.0;
	long double RG_ratio = 0.0;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			index = i * hsv.cols + j;
			h = (float)hsv.data[3 * index + 0];
			s = (float)hsv.data[3 * index + 1];
			v = (float)hsv.data[3 * index + 2];
			if (!is_weight) {
				if (abs(h - ref_h) > (180 - ref_h + h)) {
					d_g = (long double)sqrt((180 - ref_h + h) * (180 - ref_h + h) + (s - ref_s) * (s - ref_s));
				}
				else {
					d_g = (long double)sqrt((h - ref_h) * (h - ref_h) + (s - ref_s) * (s - ref_s));
				}
				d_sum += d_g;
				valid_pixel_num++;
			}
			else{
				//?if use weight 
				//if (((width / 5) < j && j < (width / 5 * 4)) && (i > height / 5&& i < height / 5 * 4)) {
					if (((width / 5) < j && j < (width / 5 * 4))) {
					if (abs(h - ref_h) > (180 - ref_h + h)) {
						d_g = (long double)sqrt((180 - ref_h + h) * (180 - ref_h + h) + (s - ref_s) * (s - ref_s));
					}
					else {
						d_g = (long double)sqrt((h - ref_h) * (h - ref_h) + (s - ref_s) * (s - ref_s));
					}
					d_sum += d_g*2;
					valid_pixel_num++;
				}
				else {
					if (abs(h - ref_h) > (180 - ref_h + h)) {
						d_g = sqrt((180 - ref_h + h) * (180 - ref_h + h) + (s - ref_s) * (s - ref_s));
					}
					else {
						d_g = sqrt((h - ref_h) * (h - ref_h) + (s - ref_s) * (s - ref_s));
					}
				
					d_sum += d_g ;
					valid_pixel_num++;
				}
			}	
			//compare two h value difference (0-180-0)
		}
	}
	//RG_ratio = d_sum / (((double)height - 1) * ((double)width - 1));
	//cout << "eeeeeee:"<<d_sum << "," << valid_pixel_num << endl;
	RG_ratio = (d_sum / valid_pixel_num);
	//cout << RG_ratio << endl;
	return  RG_ratio;
}
long double  D3Calibrate_lib::get_RG_Ratio_single_color_roi(int side) {
    //? side 0 :left red  1 :right  green
    float ref_h = (float)reference[side].hsv[0];
	float ref_s = (float)reference[side].hsv[1];
	int height, width;
	Mat hsv;

	if (side == 0) {
        height = current_image_left.height;
        width = current_image_left.width;
        hsv = current_image_left.hsv_image.clone();
	}
	else if (side == 1) {
		height = current_image_right.height;
		width = current_image_right.width;
		hsv = current_image_right.hsv_image.clone();
	}
	else {
		return -1;
	}

	long double h, s, v;
	int index = 0;
	float valid_pixel_num = 0;
	long double d_g;
	long double d_sum = 0.0;
	long double RG_ratio = 0.0;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			index = i * hsv.cols + j;
			h = (float)hsv.data[3 * index + 0];
			s = (float)hsv.data[3 * index + 1];
			v = (float)hsv.data[3 * index + 2];		
				//?if use weight 
				//if (((width / 5) < j && j < (width / 5 * 4)) && (i > height / 5 && i < height / 5 * 4)) {
                if(((width*5 /100 ) < j && j < (width *95/ 100)) ) {
					if (abs(h - ref_h) > (180 - ref_h + h)) {
						d_g = (long double)sqrt((180 - ref_h + h) * (180 - ref_h + h) + (s - ref_s) * (s - ref_s));
					}
					else {
						d_g = (long double)sqrt((h - ref_h) * (h - ref_h) + (s - ref_s) * (s - ref_s));
					}
					d_sum += d_g;
					valid_pixel_num++;
				}
			//compare two h value difference (0-180-0)
		}
	}
	//RG_ratio = d_sum / (((double)height - 1) * ((double)width - 1));
	//cout << "eeeeeee:"<<d_sum << "," << valid_pixel_num << endl;
	RG_ratio = (d_sum / valid_pixel_num);
	//cout << RG_ratio << endl;
	return  RG_ratio;
}
long double  D3Calibrate_lib::get_RG_Ratio_single_color_IT(int side, bool is_weight) {
	//? side 0 :left  1 :right
	float ref_h = (float)reference[side].hsv[0];
	float ref_s = (float)reference[side].hsv[1];
	int height, width;
	Mat hsv;
	if (side == 0) {
		height = current_image_left.height;
		width = current_image_left.width;
		hsv = current_image_left.hsv_image.clone();
	}
	else if (side == 1) {
		height = current_image_right.height;
		width = current_image_right.width;
		hsv = current_image_right.hsv_image.clone();
	}
	else {
		return -1;
	}

	long double h, s, v;
	int index = 0;
	float valid_pixel_num = 0;
	long double d_g;
	long double d_sum = 0.0;
	long double RG_ratio = 0.0;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			index = i * hsv.cols + j;
			h = (float)hsv.data[3 * index + 0];
			s = (float)hsv.data[3 * index + 1];
			v = (float)hsv.data[3 * index + 2];
			if (!is_weight) {
				if (abs(h - ref_h) > (180 - ref_h + h)) {
					d_g = (long double)sqrt((180 - ref_h + h) * (180 - ref_h + h) + (s - ref_s) * (s - ref_s));
				}
				else {
					d_g = (long double)sqrt((h - ref_h) * (h - ref_h) + (s - ref_s) * (s - ref_s));
				}
				d_sum += d_g;
				valid_pixel_num++;
			}
			else {
				//?if use weight 
				//if (((width / 5) < j && j < (width / 5* 4)) && (i > height / 5 && i < height / 5 * 4)) {
				if ((width / 5) < j && j < (width / 5 * 4)) {
					if (abs(h - ref_h) > (180 - ref_h + h)) {
						d_g = (long double)sqrt((180 - ref_h + h) * (180 - ref_h + h) + (s - ref_s) * (s - ref_s));
					}
					else {
						d_g = (long double)sqrt((h - ref_h) * (h - ref_h) + (s - ref_s) * (s - ref_s));
					}
					d_sum += d_g;
					valid_pixel_num++;
				}
			
			}
			//compare two h value difference (0-180-0)
		}
	}
	RG_ratio = (d_sum / valid_pixel_num);
	return  RG_ratio;
}
double  D3Calibrate_lib::get_Max_Color_Contrast() {

	double max_s, min_s;
	double contrast_dis;
	double ref_dis;
	int height = current_image.height;
	int width = current_image.width;
	int index;
	double max_h = 0;
	double  min_h=999;
	double temp_h;
	int max_index, min_index;
	int v_value_threshold ;
	if ( reference[0].hsv[2]> reference[1].hsv[2]) {
		v_value_threshold = reference[1].hsv[2]*0.5;
	}
	else {
		v_value_threshold = reference[0].hsv[2] * 0.5;
	}

	int s_value_threshold_min ;

	if (reference[0].hsv[1] > reference[1].hsv[1]) {
		s_value_threshold_min = reference[1].hsv[1] * 0.75;
	}
	else {
		s_value_threshold_min = reference[0].hsv[1] * 0.75;
	}

	//cout << v_value_threshold << "," << s_value_threshold_min << endl;

	std::vector<cv::Mat> hsv_planes(3);
	Mat hsv = current_image.hsv_image;
	Point minLoc, maxLoc;
	cv::split(hsv, hsv_planes);

	if (abs(reference[0].hsv[0] - reference[1].hsv[0]) > (180 - reference[0].hsv[0] + reference[1].hsv[0])) {
		ref_dis = sqrt((180 - reference[0].hsv[0] + reference[1].hsv[0]) * (180 - reference[0].hsv[0] + reference[1].hsv[0]) + (reference[0].hsv[1] - reference[1].hsv[1]) * (reference[0].hsv[1] - reference[1].hsv[1]));
	}
	else {
		ref_dis = sqrt((reference[0].hsv[0] - reference[1].hsv[0]) * (reference[0].hsv[0] - reference[1].hsv[0]) + (reference[0].hsv[1] - reference[1].hsv[1]) * (reference[0].hsv[1] - reference[1].hsv[1]));
	}

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			index = i * hsv.cols + j;

			temp_h = (double)hsv_planes[0].data[ index ];
			if (temp_h < 120) {
				temp_h = 60+ temp_h;
			}
			else {
				temp_h = temp_h-120;
			}
			if (temp_h > max_h && (double)hsv.data[3 * index + 2] > v_value_threshold &&   (double)hsv.data[3 * index + 1] > s_value_threshold_min) { 
				max_h = temp_h;
				max_index = index;
			}
			if (temp_h < min_h && (double)hsv.data[3 * index + 2] >v_value_threshold &&  (double)hsv.data[3 * index + 1] > s_value_threshold_min) {
				min_h = temp_h;
				min_index = index;
			}
		}
	}

    contrast_dis = sqrt((max_h-min_h)*(max_h - min_h)+ ((double)hsv.data[3 * max_index + 1]- (double)hsv.data[3 * min_index + 1]) * ((double)hsv.data[3 * max_index + 1] - (double)hsv.data[3 * min_index + 1]));
    return contrast_dis / ref_dis;

}

void  D3Calibrate_lib::set_ref_by_camera(Mat ref_source, char ref_type) {
	Mat tmp_frame;
	Mat tmp_hsv;
	tmp_frame = ref_source.clone();
	int index = (tmp_frame.rows / 2) * (tmp_frame.cols -1) + (tmp_frame.cols / 2);

	switch (ref_type) {
	case 'R':
		reference[0].rgb[2] = tmp_frame.data[3 * index + 0];
		reference[0].rgb[1] = tmp_frame.data[3 * index + 1];
		reference[0].rgb[0] = tmp_frame.data[3 * index + 2];
		cvtColor(tmp_frame, tmp_hsv, COLOR_BGR2HSV);
		reference[0].hsv[0] = tmp_hsv.data[3 * index + 0];
		reference[0].hsv[1] = tmp_hsv.data[3 * index + 1];
		reference[0].hsv[2] = tmp_hsv.data[3 * index + 2];
		break;

	case 'G':
		reference[1].rgb[2] = tmp_frame.data[3 * index + 0];
		 reference[1].rgb[1] = tmp_frame.data[3 * index + 1];
		 reference[1].rgb[0] = tmp_frame.data[3 * index + 2];
		cvtColor(tmp_frame, tmp_hsv, COLOR_BGR2HSV);
		reference[1].hsv[0] = tmp_hsv.data[3 * index + 0];
		reference[1].hsv[1] = tmp_hsv.data[3 * index + 1];
		reference[1].hsv[2] = tmp_hsv.data[3 * index + 2];
		break;
	}
}

int D3Calibrate_lib::find_right_and_left_camera(Mat source1,Mat source2){
    int index1 = (source1.rows / 2) * (source1.cols -1) + (source1.cols / 2);
    int index2 = (source2.rows / 2) * (source2.cols -1) + (source2.cols / 2);

    Mat tmp_hsv1,tmp_hsv2;
    cvtColor(source1, tmp_hsv1, COLOR_BGR2HSV);
    cvtColor(source2,tmp_hsv2, COLOR_BGR2HSV);

    double source1_h = tmp_hsv1.data[3 * index1 + 0];
    double source2_h = tmp_hsv2.data[3 * index2 + 0];

    if(abs(source1_h-50) > abs(source2_h-50)){
        //source 2 is green
        return 2;
    }else {
        return 1;
    }


}

int* D3Calibrate_lib::find_gray(Mat source1){
    int index = 0;
    int* gray_data = new int[3];
    int max_gray=0;
    int min_gray = 999;
    cv::cvtColor(source1, source1, cv::COLOR_BGR2GRAY);

        for(int i=0; i<source1.rows; i++){

            for(int j=0; j<source1.cols;j++){

                index = i*source1.cols+j;
                if(index ==(source1.rows / 2) * (source1.cols -1) + (source1.cols / 2)){
                        gray_data[0] = source1.data[index];
                 }
                if(source1.data[index] > max_gray){
                     max_gray = source1.data[index];
                 }
                if(source1.data[index] < min_gray){
                     min_gray = source1.data[index];
                 }
            }
        }
    gray_data[1]=max_gray;
    gray_data[2]=min_gray;

     return gray_data;

}






Mat  D3Calibrate_lib::calibration_center_locate(Mat input) {

	static int center_x;
	static int center_y;
	static int count=0;
	Mat output = input.clone();
	Mat gray;
	Mat blur_image;
	Mat binary_image;
	Mat contour_image;
	cv::Mat erode_image;
	cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1000, 1000), cv::Point(-1, -1));
	static std::vector<std::vector<cv::Point>> coutours;
	std::vector<cv::Vec4i> hierarchy;
	cv::Mat imageContours = cv::Mat::zeros(contour_image.size(), CV_8UC1);
	cv::Mat Contours = cv::Mat::zeros(contour_image.size(), CV_8UC1);
	static cv::Rect  bounding_rect;
	cv::Point text_pos;
	cv::String center_info = "";


	cv::cvtColor(output, gray, cv::COLOR_BGR2GRAY);
	cv::GaussianBlur(gray, blur_image, cv::Size(3, 3), 0);
	cv::threshold(gray, binary_image, 120, 255, cv::THRESH_BINARY);
	cv::morphologyEx(binary_image, erode_image, cv::MORPH_OPEN, kernel);
	cv::Canny(erode_image, contour_image, 150, 250);
	cv::findContours(contour_image, coutours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (!coutours.empty()) {
		bounding_rect = cv::boundingRect(coutours[0]);
		center_x = bounding_rect.x + bounding_rect.width / 2;
		center_y = bounding_rect.y + bounding_rect.height / 2;
		center_info = std::to_string(center_x) + "," + std::to_string(center_y);

		cv::drawContours(output, coutours, 0, cv::Scalar(0, 0, 255), 20, 8, hierarchy);
		cv::circle(output, Point(center_x, center_y), 20, cv::Scalar(0, 0, 255), -1);
		cv::rectangle(output, bounding_rect, cv::Scalar(0, 255, 0), 20, 8, 0);
	}
	else {
		center_info = "No White board detect";
	}

	cv::Size text_size = cv::getTextSize(center_info, cv::FONT_HERSHEY_COMPLEX, 10, 3, 0);
	text_pos.x = output.cols - text_size.width;
	text_pos.y = output.rows - text_size.height;
	cv::putText(output, center_info, text_pos, 2, 7, cv::Scalar(0, 0, 255), 3, 3, 0);

	//imshow("123", output);
	//waitKey(1);

	return output;
}

void  D3Calibrate_lib::open_camera(int camera_id ,bool *close_camera,Mat *current_frame,bool is_eye_camera) {
	VideoCapture cap;

	if (!cap.open(camera_id)) {
        std::cout << "no camera" << endl;
	} 
	else {
         *close_camera = false;

        if (is_eye_camera) {
                cap.set(CAP_PROP_FOCUS, 23);
                //cap.set(CAP_PROP_SETTINGS, 1);
                cap.set(CAP_PROP_EXPOSURE, -10);
                cap.set(CAP_PROP_FRAME_WIDTH, 1280);
                cap.set(CAP_PROP_FRAME_HEIGHT, 720);
            }
            else {
               // cap.set(CAP_PROP_SETTINGS, 1);
                cap.set(CAP_PROP_FRAME_WIDTH, 1920);
                cap.set(CAP_PROP_FRAME_HEIGHT, 1080);
            }

		while (1) {
            if (cap.read(*current_frame)) {

				if (*close_camera) {
					cap.release();
					break;
				}
            }else{               
                cap.release();
                break;
            }

		}

        *close_camera=true;
	}
}

Mat D3Calibrate_lib::get_xoff_filter(Mat src1,Mat src2) {
	Mat float_src1;
	Mat float_src2;
	cvtColor(src1, src1, COLOR_BGR2GRAY);
	src1.convertTo(float_src1, CV_32FC1);
	cvtColor(src2, src2, COLOR_BGR2GRAY);
	src2.convertTo(float_src2, CV_32FC1);
	return float_src1 - float_src2;
}
void D3Calibrate_lib::normalize_filter(normal_xoff* normalize_data,Mat filter) {
	float max_value = 0;
	float min_value = 999;
	cv::normalize(filter, normalize_data->normal_image, -1.0, 1.0, cv::NORM_MINMAX);
	
	for (int i = 0; i < filter.cols; i++) {
		if (normalize_data->normal_image.at<float>((filter.rows / 2), i, 0) > max_value)
			max_value = normalize_data->normal_image.at<float>((filter.rows / 2), i, 0);
		if (normalize_data->normal_image.at<float>((filter.rows / 2), i, 0) < min_value)
			min_value = normalize_data->normal_image.at<float>((filter.rows / 2), i, 0);
	}
	normalize_data->max = max_value;
	normalize_data->min = min_value;
	normalize_data->mean = (max_value + min_value) / 2;
	cv::threshold(normalize_data->normal_image, normalize_data->binary_image, normalize_data->mean, 1, cv::THRESH_BINARY);
	normalize_data->center.y = (filter.rows / 2) - 1;



}

Mat D3Calibrate_lib::get_crosstalk_map(Mat white_img, Mat black_img, double *mean){

    Mat result;
    Mat doubleImage_white;
    Mat doubleImage_black;

    if(white_img.rows!=black_img.rows
        || white_img.cols!=black_img.cols){
        return result;
    }
    else if(white_img.empty() || black_img.empty()){
        return result;
    }
    else{

        int height=white_img.rows;
        int width = white_img.cols;
        result.create(height, width,CV_64F);
        double tmp_ratio=0.0;
        double tmp_mean=0.0;

        //cvr to gray scale
        if(white_img.channels()!=1){
            cv::cvtColor(white_img, white_img, cv::COLOR_BGR2GRAY);
            cv::cvtColor(black_img, black_img, cv::COLOR_BGR2GRAY);
            white_img.convertTo(doubleImage_white,CV_64F);
            black_img.convertTo(doubleImage_black,CV_64F);
        }

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {

                //tmp_ratio = (black_img.at<int>(i,j)/white_img.at<int>(i,j));
                tmp_ratio = doubleImage_black.at<double>(i,j)/doubleImage_white.at<double>(i,j);
                result.at<double>(i,j) = tmp_ratio;
                *mean += tmp_ratio;
            }
        }

        *mean = tmp_mean/(height*width);

        return result;
    }

}


Mat D3Calibrate_lib::draw_crosstalk(Mat origin, Mat crosstalk_map, double ratio_step){

    Mat result;
    if(crosstalk_map.empty()){
        return result;
    }else if(origin.size!=crosstalk_map.size){
        return result;
    }
    else{
        int height=crosstalk_map.rows;
        int width = crosstalk_map.cols;
        double tmp_ratio;
        result.create(height, width, CV_8UC3);
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {

                tmp_ratio = crosstalk_map.at<double>(i,j);
                if(tmp_ratio<ratio_step){
                    result.at<cv::Vec3b>(i, j) = origin.at<cv::Vec3b>(i, j) ;
                    //result.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 0);
                }else if(2*ratio_step>tmp_ratio && tmp_ratio>ratio_step){
                    result.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 255);
                }else if(3*ratio_step>tmp_ratio && tmp_ratio>2*ratio_step){
                    result.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 255, 255);
                }else if(4*ratio_step>tmp_ratio && tmp_ratio>3*ratio_step){
                    result.at<cv::Vec3b>(i, j) = cv::Vec3b(255, 255, 0);
                }else if(14*ratio_step>tmp_ratio && tmp_ratio>4*ratio_step){
                    result.at<cv::Vec3b>(i, j) = cv::Vec3b(255, 0, 0);
                }else if(tmp_ratio>14*ratio_step){
                    result.at<cv::Vec3b>(i, j) = cv::Vec3b(255, 255, 255);
                }

            }
        }

        return result;



    }


}


/*
void D3Calibrate_lib::connect_fpga(int *res) {
	char ip[] = "172.20.10.2";
	*res = AUO3D_FPGA_Init(ip, strlen(ip));
	cout << "res:" << *res << endl;
	if (*res) {
		//float* tmp_float_data = (float*)malloc(sizeof(float)*1);
		uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
		*tmp_uns_data = 1;
		AUO3D_FPGA_SendData(PanelData::CALIBRATION, tmp_uns_data);
		set_initial_parameters();
	}
}
void  D3Calibrate_lib::set_initial_parameters() {
	AUO3D_FPGA_SendData(PanelData::FOV_H_MAX, &parameters.fov_h_max );
	AUO3D_FPGA_SendData(PanelData::FOV_H_MIN, &parameters.fov_h_min);
	AUO3D_FPGA_SendData(PanelData::FOV_V_MIN, &parameters.fov_v_min);
	AUO3D_FPGA_SendData(PanelData::FOV_V_MAX, &parameters.fov_v_max);
	AUO3D_FPGA_SendData(PanelData::FOV_VD_MAX, &parameters.fov_vd_max);
	AUO3D_FPGA_SendData(PanelData::FOV_VD_MIN, &parameters.fov_vd_min);
	AUO3D_FPGA_SendData(PanelData::CAM_OFT_X, &parameters.cam_oft_x);
	AUO3D_FPGA_SendData(PanelData::CAM_OFT_Y, &parameters.cam_oft_y);
	AUO3D_FPGA_SendData(PanelData::CAM_OFT_Z, &parameters.cam_oft_z);
	AUO3D_FPGA_SendData(PanelData::RESOLUTION_X, &parameters.resolutoin_x);
	AUO3D_FPGA_SendData(PanelData::RESOLUTION_Y, &parameters.resolution_y);
	AUO3D_FPGA_SendData(PanelData::OVD, &parameters.OVD);
	AUO3D_FPGA_SendData(PanelData::VD, &parameters.VD);
	AUO3D_FPGA_SendData(PanelData::SLANT, &parameters.slant);
	AUO3D_FPGA_SendData(PanelData::HCP, &parameters.hcp);
	AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &parameters.x_off_lens);
	AUO3D_FPGA_SendData(PanelData::WS_OVD, &parameters.WS_OVD);
	AUO3D_FPGA_SendData(PanelData::X, &parameters.X);
	AUO3D_FPGA_SendData(PanelData::Y, &parameters.Y);
	AUO3D_FPGA_SendData(PanelData::A, &parameters.A);
	AUO3D_FPGA_SendData(PanelData::B, &parameters.B);
	AUO3D_FPGA_SendData(PanelData::C, &parameters.C);
	AUO3D_FPGA_SendData(PanelData::E1, &parameters.E1);
	AUO3D_FPGA_SendData(PanelData::E2, &parameters.E2);
}

void D3Calibrate_lib::eye_tracking_update(float *x,float *y, float*z, int *refesh_rate, bool *is_get_eye_tracking) {
	int res = 0;
	float eye_position[3];
	do {
		res = AUO3D_FPGA_GetData(PanelData::EYE_POSITION, eye_position);
		if (res) {
			*x = eye_position[0];
			*y = eye_position[1];
			*z = eye_position[2];
		}
		else {
			*x = -1;
			*y = -1;
			*z = -1;
		}
		Sleep(*refesh_rate);
	} while (is_get_eye_tracking);
	
}
*/



