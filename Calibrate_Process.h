#pragma once
#pragma comment(lib,"AUO3DLib_FPGA.lib")
#include <windows.h>
#include <opencv2/highgui.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

#define DEFAUL_REF_Red_R 237
#define DEFAUL_REF_Red_G 27
#define DEFAUL_REF_Red_B 36

#define DEFAUL_REF_GREEN_B 77
#define DEFAUL_REF_GREEN_G 177
#define DEFAUL_REF_GREEN_R 35

#define INITIAL_WS_OVD 65
#define INITIAL_IT_E1 2.89
#define INITIAL_IT_E2 2.89



using namespace cv;
using namespace std;

struct CV_image {
	Mat image;
	Mat hsv_image;
	int width = 0;
	int height = 0;
};

struct Panel_parameters {

    float fov_h_min=-0;
    float fov_h_max = 0;
    float fov_v_min = 0;
    float fov_v_max = 0;
    int fov_vd_min = 0;
    int fov_vd_max = 0;
	double cam_oft_x = 0;
    double cam_oft_y = 0;
	double cam_oft_z = 0;
    int resolutoin_x = 0;
    int resolution_y = 0;
    double OVD = 0;
    double VD = 0;
	double X = 0.0;
	double Y = 0.0;
	double A1 = 0;
	double B1 = 0;
	double C1 = 0;
	double D1 = 0;
    double E1 = 0;
	double A2 = 0;
	double B2 = 0;
	double C2 = 0;
	double D2 = 0;
    double E2 = 0;
    double WS_OVD =0 ;
	double A = 0;
	double B =0;
	double C = 0;
    double x_off_lens =0;
    double slant = 0;
    double hcp = 0;
    double vd_far=0;
    int model_type=-1;
    double pixel_size = 155.25;
    double IT_default = 3807;
    string module_name="";
    string panel_id;

};

class D3Calibrate_lib
{
private:
	Mat image;
	Mat hsv_image;
	//int ref_green_rgb[3] = { NULL,NULL,NULL };
	//int ref_green_hsv[3] = { NULL,NULL,NULL };
	//int ref_red_hsv[3] = { NULL,NULL,NULL };
	//int ref_red_rgb[3] = { NULL,NULL,NULL };
	struct ref_color {
		int rgb[3] = {-1,-1,-1 };
		int hsv[3] = { -1,-1,-1 };
	};
	ref_color reference[2];



protected:
	CV_image current_image;
	CV_image current_image_left,current_image_right;
public:
	Panel_parameters parameters;
	//? Xoff Par
	struct normal_xoff {
		Mat normal_image;
		Mat binary_image;
		Point center;
		float max;
		float min;
		float mean;
	};
	Mat image_1;
	Mat image_2;
	Mat  x_off_filter;
	Mat binary_img;
	normal_xoff normalize_data;
	int wave_index;
	Mat get_xoff_filter(Mat src1, Mat src2);
	void normalize_filter(normal_xoff* normalize_data, Mat filter);
	


	//? FPGA
	void connect_fpga(int* res);
	void set_initial_parameters();
	void eye_tracking_update(float* x, float* y, float* z, int *refesh_rate, bool *is_get_eye_tracking);

	//?Camera Thread
    void open_camera(int camera_id,  bool* close_camera, Mat* current_frame, bool is_eye_camera);

	// ? Basic Image Attributes
	void image_size(int* height, int* width, int* channel);
	int set_image_by_path(string path);
	int set_image_by_camera(Mat source);
	int set_image_by_dual_camera(Mat source_left, Mat source_right);
	void get_current_image_pixelRGB_by_position(int* r, int* g, int* b, int index);
	void get_current_image_pixelHSV_by_position(int* h, int* s, int* v, int index);

	//? RG Ratio Function()
    int find_right_and_left_camera(Mat source1,Mat source2); //return dual camera num

	void set_ref_by_camera(Mat ref_source, char ref_type);
	int* get_ref_rgb(char ref_type) {
		if (ref_type == 'R' || ref_type == 'r') {
			int* rgb = reference[0].rgb;
			return rgb;
		}

		else if (ref_type == 'G' || ref_type == 'g') {
			int* rgb = reference[1].rgb;
			return rgb;
		}
	}
	int* get_ref_hsv(char ref_type) {
		if (ref_type == 'R' || ref_type == 'r') {
			int* hsv = reference[0].hsv;
			return hsv;
		}

		else if (ref_type == 'G' || ref_type == 'g') {
			int* hsv = reference[1].hsv;
			return hsv;
		}
	}

	bool is_ref_set() {
		if (reference[0].hsv[0]!=-1&&reference[1].hsv[0]!=-1)
			return true;
		else {
			return false;
		}
	}
	void Ref_RG_generate_by_default(int* red_h, int* red_s, int* green_h, int* green_s);
	double get_RG_Ratio();
	long double  get_RG_Ratio_single_color_roi(int side);
	long double get_RG_Ratio_single_color(int side, bool is_weight);
	long double  get_RG_Ratio_single_color_IT(int side, bool is_weight);
	double get_Max_Color_Contrast();
   Mat  calibration_center_locate(Mat input );

   int* D3Calibrate_lib::find_gray(Mat source1);

   Mat get_crosstalk_map(Mat white_img, Mat black_img, double *mean);
   Mat draw_crosstalk(Mat origin,Mat crosstalk_map, double ratio_step);

	
};
